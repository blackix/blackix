// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LauncherServicesPrivatePCH.h"
#include "PlatformInfo.h"
#include "ISourceCodeAccessModule.h"

#define LOCTEXT_NAMESPACE "LauncherWorker"

bool FLauncherUATTask::FirstTimeCompile = true;

/* FLauncherWorker structors
 *****************************************************************************/

FLauncherWorker::FLauncherWorker( const ITargetDeviceProxyManagerRef& InDeviceProxyManager, const ILauncherProfileRef& InProfile )
	: DeviceProxyManager(InDeviceProxyManager)
	, Profile(InProfile)
	, Status(ELauncherWorkerStatus::Busy)
{
	CreateAndExecuteTasks(InProfile);
}


/* FRunnable overrides
 *****************************************************************************/

bool FLauncherWorker::Init( )
{
	return true;
}


uint32 FLauncherWorker::Run( )
{
	FString Line;

	LaunchStartTime = FPlatformTime::Seconds();

	// wait for tasks to be completed
	while (Status == ELauncherWorkerStatus::Busy)
	{
		FPlatformProcess::Sleep(0.0f);

		FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
		if (NewLine.Len() > 0)
		{
			// process the string to break it up in to lines
			Line += NewLine;
			TArray<FString> StringArray;
			int32 count = Line.ParseIntoArray(&StringArray, TEXT("\n"), true);
			if (count > 1)
			{
				for (int32 Index = 0; Index < count-1; ++Index)
				{
					StringArray[Index].TrimTrailing();
					OutputMessageReceived.Broadcast(StringArray[Index]);
				}
                Line = StringArray[count-1];
                if (NewLine.EndsWith(TEXT("\n")))
                    Line += TEXT("\n");
			}
		}

		if (TaskChain->IsChainFinished())
		{
			Status = ELauncherWorkerStatus::Completed;

			NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			while (NewLine.Len() > 0)
			{
				// process the string to break it up in to lines
				Line += NewLine;
				TArray<FString> StringArray;
				int32 count = Line.ParseIntoArray(&StringArray, TEXT("\n"), true);
				if (count > 1)
				{
					for (int32 Index = 0; Index < count-1; ++Index)
					{
						StringArray[Index].TrimTrailing();
						OutputMessageReceived.Broadcast(StringArray[Index]);
					}
                    Line = StringArray[count-1];
                    if (NewLine.EndsWith(TEXT("\n")))
                        Line += TEXT("\n");
				}

				NewLine = FPlatformProcess::ReadPipe(ReadPipe);
			}

			// fire off the last line
			OutputMessageReceived.Broadcast(Line);

		}
	}

	// wait for tasks to be canceled
	if (Status == ELauncherWorkerStatus::Canceling)
	{
		TaskChain->Cancel();

		while (!TaskChain->IsChainFinished())
		{
			FPlatformProcess::Sleep(0.0);
		}		
	}

	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	if (Status == ELauncherWorkerStatus::Canceling)
	{
		Status = ELauncherWorkerStatus::Canceled;
		LaunchCanceled.Broadcast(FPlatformTime::Seconds() - LaunchStartTime);
	}
	else
	{
		LaunchCompleted.Broadcast(TaskChain->Succeeded(), FPlatformTime::Seconds() - LaunchStartTime, TaskChain->ReturnCode());
	}

	return 0;
}


void FLauncherWorker::Stop( )
{
	Cancel();
}


/* ILauncherWorker overrides
 *****************************************************************************/

void FLauncherWorker::Cancel( )
{
	if (Status == ELauncherWorkerStatus::Busy)
	{
		Status = ELauncherWorkerStatus::Canceling;
	}
}


int32 FLauncherWorker::GetTasks( TArray<ILauncherTaskPtr>& OutTasks ) const
{
	OutTasks.Reset();

	if (TaskChain.IsValid())
	{
		TQueue<TSharedPtr<FLauncherTask> > Queue;

		Queue.Enqueue(TaskChain);

		TSharedPtr<FLauncherTask> Task;

		// breadth first traversal
		while (Queue.Dequeue(Task))
		{
			OutTasks.Add(Task);

			const TArray<TSharedPtr<FLauncherTask> >& Continuations = Task->GetContinuations();

			for (int32 ContinuationIndex = 0; ContinuationIndex < Continuations.Num(); ++ContinuationIndex)
			{
				Queue.Enqueue(Continuations[ContinuationIndex]);
			}
		}
	}

	return OutTasks.Num();
}


void FLauncherWorker::OnTaskStarted(const FString& TaskName)
{
	StageStartTime = FPlatformTime::Seconds();
	StageStarted.Broadcast(TaskName);
}


void FLauncherWorker::OnTaskCompleted(const FString& TaskName)
{
	StageCompleted.Broadcast(TaskName, FPlatformTime::Seconds() - StageStartTime);
}


FString FLauncherWorker::CreateUATCommand( const ILauncherProfileRef& InProfile, const TArray<FString>& InPlatforms, TArray<FCommandDesc>& OutCommands, FString& CommandStart )
{
	CommandStart = TEXT("");
	FString UATCommand = TEXT("");
	static FGuid SessionId(FGuid::NewGuid());
	FString InitialMap = InProfile->GetDefaultLaunchRole()->GetInitialMap();
	if (InitialMap.IsEmpty() && InProfile->GetCookedMaps().Num() == 1)
	{
		InitialMap = InProfile->GetCookedMaps()[0];
	}

	// staging directory
	FString StageDirectory = TEXT("");
	auto PackageDirectory = InProfile->GetPackageDirectory();
	if (PackageDirectory.Len() > 0)
	{
		StageDirectory += FString::Printf(TEXT(" -stagingdirectory=\"%s\""), *PackageDirectory);
	}

	// determine if there is a server platform
	FString ServerCommand = TEXT("");
	FString ServerPlatforms = TEXT("");
	FString Platforms = TEXT("");
	FString PlatformCommand = TEXT("");
	FString OptionalParams = TEXT("");
	for (int32 PlatformIndex = 0; PlatformIndex < InPlatforms.Num(); ++PlatformIndex)
	{
		// Platform info for the given platform
		const PlatformInfo::FPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(*InPlatforms[PlatformIndex]));

		// switch server and no editor platforms to the proper type
		if (PlatformInfo->TargetPlatformName == FName("LinuxServer"))
		{
			ServerPlatforms += TEXT("+Linux");
		}
		else if (PlatformInfo->TargetPlatformName == FName("WindowsServer"))
		{
			ServerPlatforms += TEXT("+Win64");
		}
		else if (PlatformInfo->TargetPlatformName == FName("LinuxNoEditor"))
		{
			Platforms += TEXT("+Linux");
		}
		else if (PlatformInfo->TargetPlatformName == FName("WindowsNoEditor") || PlatformInfo->TargetPlatformName == FName("Windows"))
		{
			Platforms += TEXT("+Win64");
		}
		else if (PlatformInfo->TargetPlatformName == FName("MacNoEditor"))
		{
			Platforms += TEXT("+Mac");
		}
		else
		{
			Platforms += TEXT("+");
			Platforms += PlatformInfo->TargetPlatformName.ToString();
		}

		// Append any extra UAT flags specified for this platform flavor
		if (!PlatformInfo->UATCommandLine.IsEmpty())
		{
			OptionalParams += TEXT(" ");
			OptionalParams += PlatformInfo->UATCommandLine;
		}

	}
	if (ServerPlatforms.Len() > 0)
	{
		ServerCommand = TEXT(" -server -serverplatform=") + ServerPlatforms.RightChop(1);
	}
	if (Platforms.Len() > 0)
	{
		PlatformCommand = TEXT(" -platform=") + Platforms.RightChop(1);
	}

	UATCommand += PlatformCommand;
	UATCommand += ServerCommand;
	UATCommand += OptionalParams;

	// device list
	FString DeviceNames = TEXT("");
	FString DeviceCommand = TEXT("");
	FString RoleCommands = TEXT("");
	ILauncherDeviceGroupPtr DeviceGroup = InProfile->GetDeployedDeviceGroup();
	if (DeviceGroup.IsValid())
	{
		const TArray<FString>& Devices = DeviceGroup->GetDeviceIDs();
		bool bVsyncAdded = false;

		// for each deployed device...
		for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
		{
			const FString& DeviceId = Devices[DeviceIndex];

			ITargetDeviceProxyPtr DeviceProxy = DeviceProxyManager->FindProxyDeviceForTargetDevice(DeviceId);

			if (DeviceProxy.IsValid())
			{
				// add the platform
				DeviceNames += TEXT("+\"") + DeviceId + TEXT("\"");
				TArray<ILauncherProfileLaunchRolePtr> Roles;
				if (InProfile->GetLaunchRolesFor(DeviceId, Roles) > 0)
				{
					for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); RoleIndex++)
					{
						if (!bVsyncAdded && Roles[RoleIndex]->IsVsyncEnabled())
						{
							RoleCommands += TEXT(" -vsync");
							bVsyncAdded = true;
						}
						RoleCommands += *(TEXT(" ") + Roles[RoleIndex]->GetCommandLine());
					}
				}
			}			
		}
	}
	if (DeviceNames.Len() > 0)
	{
		DeviceCommand += TEXT(" -device=") + DeviceNames.RightChop(1);
	}

	// game command line
	FString CommandLine = FString::Printf(TEXT(" -cmdline=\"%s -Messaging\""),
		*InitialMap);

	// additional commands to be sent to the commandline
	FString AdditionalCommandLine = FString::Printf(TEXT(" -addcmdline=\"-SessionId=%s -SessionOwner=%s -SessionName='%s'%s\""),
		*SessionId.ToString(),
		FPlatformProcess::UserName(true),
		*InProfile->GetName(),
		*RoleCommands);

	// map list
	FString MapList = TEXT("");
	const TArray<FString>& CookedMaps = InProfile->GetCookedMaps();
	if (CookedMaps.Num() > 0 && (InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBook || InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBookInEditor))
	{
		MapList += TEXT(" -map=");
		for (int32 MapIndex = 0; MapIndex < CookedMaps.Num(); ++MapIndex)
		{
			MapList += CookedMaps[MapIndex];
			if (MapIndex+1 < CookedMaps.Num())
			{
				MapList += "+";
			}
		}
	}
	else
	{
		MapList = TEXT(" -map=") + InitialMap;
	}

	// build
	if (InProfile->IsBuilding())
	{
		UATCommand += TEXT(" -build");

		FCommandDesc Desc;
		FText Command = FText::Format(LOCTEXT("LauncherBuildDesc", "Build game for {0}"), FText::FromString(Platforms.RightChop(1)));
		Desc.Name = "Build Task";
		Desc.Desc = Command.ToString();
		Desc.EndText = TEXT("********** BUILD COMMAND COMPLETED **********");
		OutCommands.Add(Desc);
		CommandStart = TEXT("********** BUILD COMMAND STARTED **********");
		// @todo: server
	}

	// cook
	switch(InProfile->GetCookMode())
	{
	case ELauncherProfileCookModes::ByTheBook:
		{
			UATCommand += TEXT(" -cook");

			UATCommand += MapList;

			if (InProfile->IsCookingIncrementally())
			{
				UATCommand += TEXT(" -iterativecooking");
			}

			if (InProfile->IsCookingUnversioned())
			{
				UATCommand += TEXT(" -Unversioned");
			}

			FString additionalOptions = InProfile->GetCookOptions();
			if (!additionalOptions.IsEmpty())
			{
				UATCommand += TEXT(" ");
				UATCommand += additionalOptions;
			}

			if (InProfile->IsPackingWithUnrealPak())
			{
				UATCommand += TEXT(" -pak");
			}

			FCommandDesc Desc;
			FText Command = FText::Format(LOCTEXT("LauncherCookDesc", "Cook content for {0}"), FText::FromString(Platforms.RightChop(1)));
			Desc.Name = "Cook Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** COOK COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** COOK COMMAND STARTED **********");
			}
		}
		break;
	case ELauncherProfileCookModes::OnTheFly:
		{
			UATCommand += TEXT(" -cookonthefly");

//			if (InProfile->GetDeploymentMode() == ELauncherProfileDeploymentModes::DoNotDeploy)
			{
				UATCommand += " -nokill";
			}
			UATCommand += MapList;

			FCommandDesc Desc;
			FText Command = LOCTEXT("LauncherCookDesc", "Starting cook on the fly server");
			Desc.Name = "Cook Server Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** COOK COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** COOK COMMAND STARTED **********");
			}
		}
		break;
	case ELauncherProfileCookModes::ByTheBookInEditor:
		UATCommand += MapList;
		if (InProfile->IsCookingIncrementally())
		{
			UATCommand += TEXT(" -iterativecooking");
		}
	case ELauncherProfileCookModes::DoNotCook:
		UATCommand += TEXT(" -skipcook");
		break;
	}

	// stage/package/deploy
	if (InProfile->GetDeploymentMode() != ELauncherProfileDeploymentModes::DoNotDeploy)
	{
		switch (InProfile->GetDeploymentMode())
		{
		case ELauncherProfileDeploymentModes::CopyRepository:
			{
				UATCommand += TEXT(" -skipstage -deploy");
				UATCommand += CommandLine;
				UATCommand += StageDirectory;
				UATCommand += DeviceCommand;
				UATCommand += AdditionalCommandLine;

				FCommandDesc Desc;
				FText Command = FText::Format(LOCTEXT("LauncherDeployDesc", "Deploying content for {0}"), FText::FromString(Platforms.RightChop(1)));
				Desc.Name = "Deploy Task";
				Desc.Desc = Command.ToString();
				Desc.EndText = TEXT("********** DEPLOY COMMAND COMPLETED **********");
				OutCommands.Add(Desc);
				if (CommandStart.Len() == 0)
				{
					CommandStart = TEXT("********** DEPLOY COMMAND STARTED **********");
				}
			}
			break;

		case ELauncherProfileDeploymentModes::CopyToDevice:
			{
				if (Profile->IsDeployingIncrementally())
				{
					UATCommand += " -iterativedeploy";
				}
			}
		case ELauncherProfileDeploymentModes::FileServer:
			{
				UATCommand += TEXT(" -stage -deploy");
				UATCommand += CommandLine;
				UATCommand += StageDirectory;
				UATCommand += DeviceCommand;
				UATCommand += AdditionalCommandLine;

				FCommandDesc Desc;
				FText Command = FText::Format(LOCTEXT("LauncherDeployDesc", "Deploying content for {0}"), FText::FromString(Platforms.RightChop(1)));
				Desc.Name = "Deploy Task";
				Desc.Desc = Command.ToString();
				Desc.EndText = TEXT("********** DEPLOY COMMAND COMPLETED **********");
				OutCommands.Add(Desc);
				if (CommandStart.Len() == 0)
				{
					CommandStart = TEXT("********** STAGE COMMAND STARTED **********");
				}
			}
			break;
		}

		// run
		if (InProfile->GetLaunchMode() != ELauncherProfileLaunchModes::DoNotLaunch)
		{
			UATCommand += TEXT(" -run -nokill");

			FCommandDesc Desc;
			FText Command = FText::Format(LOCTEXT("LauncherRunDesc", "Launching on {0}"), FText::FromString(DeviceNames.RightChop(1)));
			Desc.Name = "Run Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** RUN COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** RUN COMMAND STARTED **********");
			}
		}
	}
	else
	{
		if (InProfile->GetPackagingMode() == ELauncherProfilePackagingModes::Locally)
		{
			UATCommand += TEXT(" -stage -package");
			UATCommand += StageDirectory;
			UATCommand += CommandLine;
			UATCommand += AdditionalCommandLine;

			FCommandDesc Desc;
			FText Command = FText::Format(LOCTEXT("LauncherPackageDesc", "Packaging content for {0}"), FText::FromString(Platforms.RightChop(1)));
			Desc.Name = "Package Task";
			Desc.Desc = Command.ToString();
			Desc.EndText = TEXT("********** PACKAGE COMMAND COMPLETED **********");
			OutCommands.Add(Desc);
			if (CommandStart.Len() == 0)
			{
				CommandStart = TEXT("********** STAGE COMMAND STARTED **********");
			}
		}
	}

	// wait for completion of UAT
	FCommandDesc Desc;
	FText Command = LOCTEXT("LauncherCompletionDesc", "UAT post launch cleanup");
	Desc.Name = "Post Launch Task";
	Desc.Desc = Command.ToString();
	Desc.EndText = TEXT("********** LAUNCH COMPLETED **********");
	OutCommands.Add(Desc);

	return UATCommand;
}

/* FLauncherWorker implementation
 *****************************************************************************/

void FLauncherWorker::CreateAndExecuteTasks( const ILauncherProfileRef& InProfile )
{
	// check to see if we need to build by default
	if (!InProfile->HasProjectSpecified())
	{
		FString ProjectPath = FPaths::GetPath(InProfile->GetProjectPath());
		TArray<FString> OutProjectCodeFilenames;
		IFileManager::Get().FindFilesRecursive(OutProjectCodeFilenames, *(ProjectPath / TEXT("Source")), TEXT("*.h"), true, false, false);
		IFileManager::Get().FindFilesRecursive(OutProjectCodeFilenames, *(ProjectPath / TEXT("Source")), TEXT("*.cpp"), true, false, false);
		ISourceCodeAccessModule& SourceCodeAccessModule = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
		if (OutProjectCodeFilenames.Num() > 0 && SourceCodeAccessModule.GetAccessor().CanAccessSourceCode())
		{
			InProfile->SetBuildGame(true);
		}
	}
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	// create task chains
	TaskChain = MakeShareable(new FLauncherVerifyProfileTask());
	TArray<FString> Platforms;
	if (InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBook || InProfile->IsBuilding())
	{
		Platforms = InProfile->GetCookedPlatforms();
	}

	FLauncherUATTask::FirstTimeCompile = true;

	// determine deployment platforms
	ILauncherDeviceGroupPtr DeviceGroup = InProfile->GetDeployedDeviceGroup();
	FName Variant = NAME_None;

	if (DeviceGroup.IsValid() && Platforms.Num() < 1)
	{
		const TArray<FString>& Devices = DeviceGroup->GetDeviceIDs();
		// for each deployed device...
		for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
		{
			const FString& DeviceId = Devices[DeviceIndex];

			ITargetDeviceProxyPtr DeviceProxy = DeviceProxyManager->FindProxyDeviceForTargetDevice(DeviceId);

			if (DeviceProxy.IsValid())
			{
				// add the platform
				Variant = DeviceProxy->GetTargetDeviceVariant(DeviceId);
				Platforms.AddUnique(DeviceProxy->GetTargetPlatformName(Variant));
			}			
		}
	}

#if !WITH_EDITOR
	// can't cook by the book in the editor if we are not in the editor...
	check( InProfile->GetCookMode() != ELauncherProfileCookModes::ByTheBookInEditor );
#endif

	TSharedPtr<FLauncherTask> NextTask = TaskChain;
	if (InProfile->GetCookMode() == ELauncherProfileCookModes::ByTheBookInEditor)
	{
		// need a command which will wait for the cook to finish
		class FWaitForCookInEditorToFinish : public FLauncherTask
		{
		public:
			FWaitForCookInEditorToFinish() : FLauncherTask( FString(TEXT("Cooking in the editor")), FString(TEXT("Prepairing content to run on device")), NULL, NULL)
			{
			}
			virtual bool PerformTask( FLauncherTaskChainState& ChainState ) override
			{
				while ( !ChainState.Profile->OnIsCookFinished().Execute() )
				{
					if (GetStatus() == ELauncherTaskStatus::Canceling)
					{
						ChainState.Profile->OnCookCanceled().Execute();
						return false;
					}
					FPlatformProcess::Sleep( 0.1f );
				}
				return true;
			}
		};
		TSharedPtr<FLauncherTask> WaitTask = MakeShareable(new FWaitForCookInEditorToFinish());
		WaitTask->OnStarted().AddRaw(this, &FLauncherWorker::OnTaskStarted);
		WaitTask->OnCompleted().AddRaw(this, &FLauncherWorker::OnTaskCompleted);
		NextTask->AddContinuation(WaitTask);
		NextTask = WaitTask;
	}
	TArray<FCommandDesc> Commands;
	FString StartString;
	FString UATCommand = CreateUATCommand(InProfile, Platforms, Commands, StartString);
	TSharedPtr<FLauncherTask> BuildTask = MakeShareable(new FLauncherUATTask(UATCommand, TEXT("Build Task"), TEXT("Launching UAT..."), ReadPipe, WritePipe, InProfile->GetEditorExe(), ProcHandle, this, StartString));
	BuildTask->OnStarted().AddRaw(this, &FLauncherWorker::OnTaskStarted);
	BuildTask->OnCompleted().AddRaw(this, &FLauncherWorker::OnTaskCompleted);
	NextTask->AddContinuation(BuildTask);
	NextTask = BuildTask;
	for (int32 Index = 0; Index < Commands.Num(); ++Index)
	{
		class FLauncherWaitTask : public FLauncherTask
		{
		public:
			FLauncherWaitTask( const FString& InCommandEnd, const FString& InName, const FString& InDesc, FProcHandle& InProcessHandle, ILauncherWorker* InWorker)
				: FLauncherTask(InName, InDesc, 0, 0)
				, CommandText(InCommandEnd)
				, ProcessHandle(InProcessHandle)
			{
				EndTextFound = false;
				InWorker->OnOutputReceived().AddRaw(this, &FLauncherWaitTask::HandleOutputReceived);
			}

		protected:
			virtual bool PerformTask( FLauncherTaskChainState& ChainState ) override
			{
				while (FPlatformProcess::IsProcRunning(ProcessHandle) && !EndTextFound)
				{
					if (GetStatus() == ELauncherTaskStatus::Canceling)
					{
						FPlatformProcess::TerminateProc(ProcessHandle, true);
						return false;
					}
					FPlatformProcess::Sleep(0.25);
				}
				if (!EndTextFound && !FPlatformProcess::GetProcReturnCode(ProcessHandle, &Result))
				{
					return false;
				}
				return (Result == 0);
			}

			void HandleOutputReceived(const FString& InMessage)
			{
				EndTextFound |= InMessage.Contains(CommandText);
			}

		private:
			FString CommandText;
			FProcHandle& ProcessHandle;
			bool EndTextFound;
		};			

		TSharedPtr<FLauncherTask> WaitTask = MakeShareable(new FLauncherWaitTask(Commands[Index].EndText, Commands[Index].Name, Commands[Index].Desc, ProcHandle, this));
		WaitTask->OnStarted().AddRaw(this, &FLauncherWorker::OnTaskStarted);
		WaitTask->OnCompleted().AddRaw(this, &FLauncherWorker::OnTaskCompleted);
		NextTask->AddContinuation(WaitTask);
		NextTask = WaitTask;
	}

	// execute the chain
	FLauncherTaskChainState ChainState;

	ChainState.Profile = InProfile;
	ChainState.SessionId = FGuid::NewGuid();

	TaskChain->Execute(ChainState);
}

#undef LOCTEXT_NAMESPACE
