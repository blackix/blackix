#include "OculusBuildAnalytics.h"
#include "GameProjectGenerationModule.h"

FOculusBuildAnalytics* FOculusBuildAnalytics::instance = 0;

FOculusBuildAnalytics* FOculusBuildAnalytics::GetInstance()
{
	if (instance == 0)
	{
		instance = new FOculusBuildAnalytics();
	}

	return instance;
}

FOculusBuildAnalytics::FOculusBuildAnalytics()
{
	ILauncherServicesModule& ProjectLauncherServicesModule = FModuleManager::LoadModuleChecked<ILauncherServicesModule>("LauncherServices");
	ProjectLauncherServicesModule.OnCreateLauncherDelegate.AddRaw(this, &FOculusBuildAnalytics::OnLauncherCreated);
}

void FOculusBuildAnalytics::OnLauncherCreated(ILauncherRef Launcher)
{
	// Add callback for when launcher worker is started
	Launcher->FLauncherWorkerStartedDelegate.AddRaw(this, &FOculusBuildAnalytics::OnLauncherWorkerStarted);
}

void FOculusBuildAnalytics::OnLauncherWorkerStarted(ILauncherWorkerPtr LauncherWorker, ILauncherProfileRef Profile)
{
	TArray<FString> Platforms = Profile.Get().GetCookedPlatforms();
	if (Platforms.Num() == 1)
	{
		if (Platforms[0].Equals("Android_ASTC") || Platforms[0].Contains("Windows"))
		{
			CurrentBuildStage = UNDEFINED_STAGE;
			AndroidPackageTime = 0;
			UATLaunched = false;
			CurrentBuildPlatform = Platforms[0];

			// Assign callbacks for stages
			LauncherWorker.Get()->OnStageCompleted().AddRaw(this, &FOculusBuildAnalytics::OnStageCompleted);
			LauncherWorker.Get()->OnOutputReceived().AddRaw(this, &FOculusBuildAnalytics::OnBuildOutputRecieved);
			LauncherWorker.Get()->OnStageStarted().AddRaw(this, &FOculusBuildAnalytics::OnStageStarted);

			// Get information on what oculus platform we are building for and also the OS platform
			FString OculusPlatform;
			if (CurrentBuildPlatform.Equals("Android_ASTC"))
			{
				UEnum* OculusMobileDevices = StaticEnum<EOculusMobileDevice::Type>();
				UAndroidRuntimeSettings* Settings = GetMutableDefault<UAndroidRuntimeSettings>();
				TArray<TEnumAsByte<EOculusMobileDevice::Type>> TargetOculusDevices = Settings->PackageForOculusMobile;
				TArray<FString> Devices;

				if (TargetOculusDevices.Contains(EOculusMobileDevice::GearGo))
				{
					Devices.Add("geargo");
				}
				if (TargetOculusDevices.Contains(EOculusMobileDevice::Quest))
				{
					Devices.Add("quest");
				}
				OculusPlatform = FString::Join(Devices, TEXT("_"));
			}
			else if (CurrentBuildPlatform.Contains("Windows"))
			{
				CurrentBuildPlatform = "Windows";
				OculusPlatform = "rift";
			}

			// Count user asset files
			UserAssetCount = 0;
			TArray<FString> FileNames;
			IFileManager::Get().FindFilesRecursive(FileNames, *FPaths::ProjectContentDir(), TEXT("*.*"), true, false, false);
			UserAssetCount = FileNames.Num();

			// Count user script files
			FGameProjectGenerationModule& GameProjectModule = FModuleManager::LoadModuleChecked<FGameProjectGenerationModule>(TEXT("GameProjectGeneration"));
			SourceFileCount = 0;
			SourceFileDirectorySize = 0;
			GameProjectModule.Get().GetProjectSourceDirectoryInfo(SourceFileCount, SourceFileDirectorySize);

			// Send build start event with corresponding metadata
			ovrp_AddCustomMetadata("asset_count", TCHAR_TO_ANSI(*FString::FromInt(UserAssetCount)));
			ovrp_AddCustomMetadata("script_count", TCHAR_TO_ANSI(*FString::FromInt(SourceFileCount)));

			ovrp_AddCustomMetadata("target_platform", TCHAR_TO_ANSI(*CurrentBuildPlatform));
			ovrp_AddCustomMetadata("target_oculus_platform", TCHAR_TO_ANSI(*OculusPlatform));

			TArray<ILauncherTaskPtr> TaskList;
			LauncherWorker->GetTasks(TaskList);

			ovrp_SendEvent2("build_start",TCHAR_TO_ANSI(*FString::FromInt(TaskList.Num())), "ovrbuild");
		}
	}
}

void FOculusBuildAnalytics::OnStageCompleted(const FString& StageName, double Time)
{
	if (CurrentBuildStage != UNDEFINED_STAGE)
	{
		FString TaskName;
		switch (CurrentBuildStage)
		{
			case COOK_IN_EDITOR_STAGE:	TaskName = "build_step_editor_cook";	break;
			case LAUNCH_UAT_STAGE:		TaskName = "build_step_launch_uat";		break;
			case COMPILE_STAGE:			TaskName = "build_step_compile";		break;
			case COOK_STAGE:			TaskName = "build_step_cook";			break;
			case DEPLOY_STAGE:			TaskName = "build_step_deploy";			break;
			case PACKAGE_STAGE:			TaskName = "build_step_package";		break;
		}

		if (AndroidPackageTime > 0)
		{
			Time -= AndroidPackageTime;
		}

		ovrp_SendEvent2(TCHAR_TO_ANSI(*TaskName), TCHAR_TO_ANSI(*FString::SanitizeFloat(Time)), "ovrbuild");
	}
}

void FOculusBuildAnalytics::OnStageStarted(const FString& StageName)
{
	if (StageName.Equals("Cooking in the editor"))
	{
		CurrentBuildStage = COOK_IN_EDITOR_STAGE;
	}
	else if (StageName.Equals("Build Task") && CurrentBuildStage == LAUNCH_UAT_STAGE)
	{
		CurrentBuildStage = COMPILE_STAGE;
	}
	else if (StageName.Equals("Build Task"))
	{
		CurrentBuildStage = LAUNCH_UAT_STAGE;
	}
	else if (StageName.Equals("Cook Task"))
	{
		CurrentBuildStage = COOK_STAGE;
	}
	else if (StageName.Equals("Package Task"))
	{
		CurrentBuildStage = PACKAGE_STAGE;
	}
	else if (StageName.Equals("Deploy Task"))
	{
		CurrentBuildStage = DEPLOY_STAGE;
	}
	else
	{
		CurrentBuildStage = UNDEFINED_STAGE;
	}
}

void FOculusBuildAnalytics::OnBuildOutputRecieved(const FString& Message)
{
	if (CurrentBuildPlatform.Equals("Android_ASTC") && (CurrentBuildStage == DEPLOY_STAGE || CurrentBuildStage == PACKAGE_STAGE))
	{
		if (Message.Contains("BUILD SUCCESSFUL"))
		{
			FString Text, Time;
			Message.Split("in", &Text, &Time);

			if (!Time.IsEmpty())
			{
				FString SMinutes, SSeconds;
				if (Time.Contains("m"))
				{
					Time.Split("m", &SMinutes, &SSeconds);
				}
				else
				{
					SSeconds = Time;
				}

				int Minutes = FCString::Atoi(*SMinutes);
				int Seconds = FCString::Atoi(*SSeconds);

				AndroidPackageTime = Minutes * 60 + Seconds;

				ovrp_SendEvent2("build_step_gradle_build", TCHAR_TO_ANSI(*FString::SanitizeFloat(AndroidPackageTime)), "ovrbuild");
			}
		}
	}
}