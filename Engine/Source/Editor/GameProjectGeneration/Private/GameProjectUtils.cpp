// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#include "GameProjectGenerationPrivatePCH.h"
#include "UnrealEdMisc.h"
#include "ISourceControlModule.h"
#include "MainFrame.h"
#include "DefaultTemplateProjectDefs.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "EngineBuildSettings.h"

#include "DesktopPlatformModule.h"
#include "TargetPlatform.h"

#include "ClassIconFinder.h"
#include "Editor/UnrealEd/Public/SourceCodeNavigation.h"

#include "UProjectInfo.h"
#include "DesktopPlatformModule.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "GameFramework/GameMode.h"
#include "HotReloadInterface.h"
#include "SVerbChoiceDialog.h"
#include "SourceCodeNavigation.h"


#define LOCTEXT_NAMESPACE "GameProjectUtils"

#define MAX_PROJECT_PATH_BUFFER_SPACE 130 // Leave a reasonable buffer of additional characters to account for files created in the content directory during or after project generation
#define MAX_PROJECT_NAME_LENGTH 20 // Enforce a reasonable project name length so the path is not too long for PLATFORM_MAX_FILEPATH_LENGTH
static_assert(PLATFORM_MAX_FILEPATH_LENGTH - MAX_PROJECT_PATH_BUFFER_SPACE > 0, "File system path shorter than project creation buffer space.");

#define MAX_CLASS_NAME_LENGTH 32 // Enforce a reasonable class name length so the path is not too long for PLATFORM_MAX_FILEPATH_LENGTH

TWeakPtr<SNotificationItem> GameProjectUtils::UpdateGameProjectNotification = NULL;
TWeakPtr<SNotificationItem> GameProjectUtils::WarningProjectNameNotification = NULL;

FString GameProjectUtils::FNewClassInfo::GetClassName() const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		return BaseClass ? FName::NameToDisplayString(BaseClass->GetName(), false) : TEXT("");

	case EClassType::EmptyCpp:
		return TEXT("None");

	case EClassType::SlateWidget:
		return TEXT("Slate Widget");

	case EClassType::SlateWidgetStyle:
		return TEXT("Slate Widget Style");

	default:
		break;
	}
	return TEXT("");
}

FString GameProjectUtils::FNewClassInfo::GetClassDescription() const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		{
			if(BaseClass)
			{
				FString ClassDescription = BaseClass->GetToolTipText().ToString();
				int32 FullStopIndex = 0;
				if(ClassDescription.FindChar('.', FullStopIndex))
				{
					// Only show the first sentence so as not to clutter up the UI with a detailed description of implementation details
					ClassDescription = ClassDescription.Left(FullStopIndex + 1);
				}

				// Strip out any new-lines in the description
				ClassDescription = ClassDescription.Replace(TEXT("\n"), TEXT(" "));
				return ClassDescription;
			}
		}
		break;

	case EClassType::EmptyCpp:
		return TEXT("An empty C++ class with a default constructor and destructor");

	case EClassType::SlateWidget:
		return TEXT("A custom Slate widget, deriving from SCompoundWidget");

	case EClassType::SlateWidgetStyle:
		return TEXT("A custom Slate widget style, deriving from FSlateWidgetStyle, along with its associated UObject wrapper class");

	default:
		break;
	}
	return TEXT("");
}

const FSlateBrush* GameProjectUtils::FNewClassInfo::GetClassIcon() const
{
	// Safe to do even if BaseClass is null, since FindIconForClass will return the default icon
	return FClassIconFinder::FindIconForClass(BaseClass);
}

FString GameProjectUtils::FNewClassInfo::GetClassPrefixCPP() const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		return BaseClass ? BaseClass->GetPrefixCPP() : TEXT("");

	case EClassType::EmptyCpp:
		return TEXT("");

	case EClassType::SlateWidget:
		return TEXT("S");

	case EClassType::SlateWidgetStyle:
		return TEXT("F");

	default:
		break;
	}
	return TEXT("");
}

FString GameProjectUtils::FNewClassInfo::GetClassNameCPP() const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		return BaseClass ? BaseClass->GetName() : TEXT("");

	case EClassType::EmptyCpp:
		return TEXT("");

	case EClassType::SlateWidget:
		return TEXT("CompoundWidget");

	case EClassType::SlateWidgetStyle:
		return TEXT("SlateWidgetStyle");

	default:
		break;
	}
	return TEXT("");
}

FString GameProjectUtils::FNewClassInfo::GetCleanClassName(const FString& ClassName) const
{
	FString CleanClassName = ClassName;

	switch(ClassType)
	{
	case EClassType::SlateWidgetStyle:
		{
			// Slate widget style classes always take the form FMyThingWidget, and UMyThingWidgetStyle
			// if our class ends with either Widget or WidgetStyle, we need to strip those out to avoid silly looking duplicates
			if(CleanClassName.EndsWith(TEXT("Style")))
			{
				CleanClassName = CleanClassName.LeftChop(5); // 5 for "Style"
			}
			if(CleanClassName.EndsWith(TEXT("Widget")))
			{
				CleanClassName = CleanClassName.LeftChop(6); // 6 for "Widget"
			}
		}
		break;

	default:
		break;
	}

	return CleanClassName;
}

FString GameProjectUtils::FNewClassInfo::GetFinalClassName(const FString& ClassName) const
{
	const FString CleanClassName = GetCleanClassName(ClassName);

	switch(ClassType)
	{
	case EClassType::SlateWidgetStyle:
		return FString::Printf(TEXT("%sWidgetStyle"), *CleanClassName);

	default:
		break;
	}

	return CleanClassName;
}

bool GameProjectUtils::FNewClassInfo::GetIncludePath(FString& OutIncludePath) const
{
	switch(ClassType)
	{
	case EClassType::UObject:
		if(BaseClass && BaseClass->HasMetaData(TEXT("IncludePath")))
		{
			OutIncludePath = BaseClass->GetMetaData(TEXT("IncludePath"));
			return true;
		}
		break;

	default:
		break;
	}
	return false;
}

FString GameProjectUtils::FNewClassInfo::GetHeaderFilename(const FString& ClassName) const
{
	const FString HeaderFilename = GetFinalClassName(ClassName) + TEXT(".h");

	switch(ClassType)
	{
	case EClassType::SlateWidget:
		return TEXT("S") + HeaderFilename;

	default:
		break;
	}

	return HeaderFilename;
}

FString GameProjectUtils::FNewClassInfo::GetSourceFilename(const FString& ClassName) const
{
	const FString SourceFilename = GetFinalClassName(ClassName) + TEXT(".cpp");

	switch(ClassType)
	{
	case EClassType::SlateWidget:
		return TEXT("S") + SourceFilename;

	default:
		break;
	}

	return SourceFilename;
}

FString GameProjectUtils::FNewClassInfo::GetHeaderTemplateFilename() const
{
	switch(ClassType)
	{
		case EClassType::UObject:
		{
			if( BaseClass != nullptr && ( BaseClass == UActorComponent::StaticClass() || BaseClass == USceneComponent::StaticClass() ) )
			{
				return TEXT( "ActorComponentClass.h.template" );
			}
			else if( BaseClass != nullptr && BaseClass == AActor::StaticClass() )
			{
				return TEXT( "ActorClass.h.template" );
			}
			else
			{
				// Some other non-actor, non-component UObject class
				return TEXT( "UObjectClass.h.template" );
			}
		}

	case EClassType::EmptyCpp:
		return TEXT("EmptyClass.h.template");

	case EClassType::SlateWidget:
		return TEXT("SlateWidget.h.template");

	case EClassType::SlateWidgetStyle:
		return TEXT("SlateWidgetStyle.h.template");

	default:
		break;
	}
	return TEXT("");
}

FString GameProjectUtils::FNewClassInfo::GetSourceTemplateFilename() const
{
	switch(ClassType)
	{
		case EClassType::UObject:
			if( BaseClass != nullptr && ( BaseClass == UActorComponent::StaticClass() || BaseClass == USceneComponent::StaticClass() ) )
			{
				return TEXT( "ActorComponentClass.cpp.template" );
			}
			else if( BaseClass != nullptr && BaseClass == AActor::StaticClass() )
			{
				return TEXT( "ActorClass.cpp.template" );
			}
			else
			{
				// Some other non-actor, non-component UObject class
				return TEXT( "UObjectClass.cpp.template" );
			}
	
	case EClassType::EmptyCpp:
		return TEXT("EmptyClass.cpp.template");

	case EClassType::SlateWidget:
		return TEXT("SlateWidget.cpp.template");

	case EClassType::SlateWidgetStyle:
		return TEXT("SlateWidgetStyle.cpp.template");

	default:
		break;
	}
	return TEXT("");
}

bool GameProjectUtils::IsValidProjectFileForCreation(const FString& ProjectFile, FText& OutFailReason)
{
	const FString BaseProjectFile = FPaths::GetBaseFilename(ProjectFile);
	if ( FPaths::GetPath(ProjectFile).IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectPath", "You must specify a path." );
		return false;
	}

	if ( BaseProjectFile.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectName", "You must specify a project name." );
		return false;
	}

	if ( BaseProjectFile.Contains(TEXT(" ")) )
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsSpace", "Project names may not contain a space." );
		return false;
	}

	if ( !FChar::IsAlpha(BaseProjectFile[0]) )
	{
		OutFailReason = LOCTEXT( "ProjectNameMustBeginWithACharacter", "Project names must begin with an alphabetic character." );
		return false;
	}

	if ( BaseProjectFile.Len() > MAX_PROJECT_NAME_LENGTH )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaxProjectNameLength"), MAX_PROJECT_NAME_LENGTH );
		OutFailReason = FText::Format( LOCTEXT( "ProjectNameTooLong", "Project names must not be longer than {MaxProjectNameLength} characters." ), Args );
		return false;
	}

	const int32 MaxProjectPathLength = PLATFORM_MAX_FILEPATH_LENGTH - MAX_PROJECT_PATH_BUFFER_SPACE;
	if ( FPaths::GetBaseFilename(ProjectFile, false).Len() > MaxProjectPathLength )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaxProjectPathLength"), MaxProjectPathLength );
		OutFailReason = FText::Format( LOCTEXT( "ProjectPathTooLong", "A project's path must not be longer than {MaxProjectPathLength} characters." ), Args );
		return false;
	}

	if ( FPaths::GetExtension(ProjectFile) != FProjectDescriptor::GetExtension() )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFileExtension"), FText::FromString( FProjectDescriptor::GetExtension() ) );
		OutFailReason = FText::Format( LOCTEXT( "InvalidProjectFileExtension", "File extension is not {ProjectFileExtension}" ), Args );
		return false;
	}

	FString IllegalNameCharacters;
	if ( !NameContainsOnlyLegalCharacters(BaseProjectFile, IllegalNameCharacters) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("IllegalNameCharacters"), FText::FromString( IllegalNameCharacters ) );
		OutFailReason = FText::Format( LOCTEXT( "ProjectNameContainsIllegalCharacters", "Project names may not contain the following characters: {IllegalNameCharacters}" ), Args );
		return false;
	}

	if (NameContainsUnderscoreAndXB1Installed(BaseProjectFile))
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsIllegalCharactersOnXB1", "Project names may not contain an underscore when the Xbox One XDK is installed." );
		return false;
	}

	if ( !FPaths::ValidatePath(FPaths::GetPath(ProjectFile), &OutFailReason) )
	{
		return false;
	}

	if ( ProjectFileExists(ProjectFile) )
	{
		OutFailReason = LOCTEXT( "ProjectFileAlreadyExists", "This project file already exists." );
		return false;
	}

	if ( FPaths::ConvertRelativePathToFull(FPaths::GetPath(ProjectFile)).StartsWith( FPaths::ConvertRelativePathToFull(FPaths::EngineDir())) )
	{
		OutFailReason = LOCTEXT( "ProjectFileCannotBeUnderEngineFolder", "Project cannot be saved under the Engine folder. Please choose a different directory." );
		return false;
	}

	if ( AnyProjectFilesExistInFolder(FPaths::GetPath(ProjectFile)) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFileExtension"), FText::FromString( FProjectDescriptor::GetExtension() ) );
		OutFailReason = FText::Format( LOCTEXT( "AProjectFileAlreadyExistsAtLoction", "Another .{ProjectFileExtension} file already exists in the specified folder" ), Args );
		return false;
	}

	return true;
}

bool GameProjectUtils::OpenProject(const FString& ProjectFile, FText& OutFailReason)
{
	if ( ProjectFile.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectFileSpecified", "You must specify a project file." );
		return false;
	}

	const FString BaseProjectFile = FPaths::GetBaseFilename(ProjectFile);
	if ( BaseProjectFile.Contains(TEXT(" ")) )
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsSpace", "Project names may not contain a space." );
		return false;
	}

	if ( !FChar::IsAlpha(BaseProjectFile[0]) )
	{
		OutFailReason = LOCTEXT( "ProjectNameMustBeginWithACharacter", "Project names must begin with an alphabetic character." );
		return false;
	}

	const int32 MaxProjectPathLength = PLATFORM_MAX_FILEPATH_LENGTH - MAX_PROJECT_PATH_BUFFER_SPACE;
	if ( FPaths::GetBaseFilename(ProjectFile, false).Len() > MaxProjectPathLength )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaxProjectPathLength"), MaxProjectPathLength );
		OutFailReason = FText::Format( LOCTEXT( "ProjectPathTooLong", "A project's path must not be longer than {MaxProjectPathLength} characters." ), Args );
		return false;
	}

	if ( FPaths::GetExtension(ProjectFile) != FProjectDescriptor::GetExtension() )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFileExtension"), FText::FromString( FProjectDescriptor::GetExtension() ) );
		OutFailReason = FText::Format( LOCTEXT( "InvalidProjectFileExtension", "File extension is not {ProjectFileExtension}" ), Args );
		return false;
	}

	FString IllegalNameCharacters;
	if ( !NameContainsOnlyLegalCharacters(BaseProjectFile, IllegalNameCharacters) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("IllegalNameCharacters"), FText::FromString( IllegalNameCharacters ) );
		OutFailReason = FText::Format( LOCTEXT( "ProjectNameContainsIllegalCharacters", "Project names may not contain the following characters: {IllegalNameCharacters}" ), Args );
		return false;
	}

	if (NameContainsUnderscoreAndXB1Installed(BaseProjectFile))
	{
		OutFailReason = LOCTEXT( "ProjectNameContainsIllegalCharactersOnXB1", "Project names may not contain an underscore when the Xbox One XDK is installed." );
		return false;
	}

	if ( !FPaths::ValidatePath(FPaths::GetPath(ProjectFile), &OutFailReason) )
	{
		return false;
	}

	if ( !ProjectFileExists(ProjectFile) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ProjectFile"), FText::FromString( ProjectFile ) );
		OutFailReason = FText::Format( LOCTEXT( "ProjectFileDoesNotExist", "{ProjectFile} does not exist." ), Args );
		return false;
	}

	FUnrealEdMisc::Get().SwitchProject(ProjectFile, false);

	return true;
}

bool GameProjectUtils::OpenCodeIDE(const FString& ProjectFile, FText& OutFailReason)
{
	if ( ProjectFile.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoProjectFileSpecified", "You must specify a project file." );
		return false;
	}

	// Check whether this project is a foreign project. Don't use the cached project dictionary; we may have just created a new project.
	FString SolutionFolder;
	FString SolutionFilenameWithoutExtension;
	if( FUProjectDictionary(FPaths::RootDir()).IsForeignProject(ProjectFile) )
	{
		SolutionFolder = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::GetPath(ProjectFile));
		SolutionFilenameWithoutExtension = FPaths::GetBaseFilename(ProjectFile);
	}
	else
	{
		SolutionFolder = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FPaths::RootDir());
		SolutionFilenameWithoutExtension = TEXT("UE4");
	}

	// Get the solution filename
	FString CodeSolutionFile;
#if PLATFORM_WINDOWS
	CodeSolutionFile = SolutionFilenameWithoutExtension + TEXT(".sln");
#elif PLATFORM_MAC
	CodeSolutionFile = SolutionFilenameWithoutExtension + TEXT(".xcodeproj");
#elif PLATFORM_LINUX
	// FIXME: need a better way to select between plugins. For now we don't generate .kdev4 directly. Should depend on PreferredAccessor setting
	CodeSolutionFile = SolutionFilenameWithoutExtension + TEXT(".pro");
#else
	OutFailReason = LOCTEXT( "OpenCodeIDE_UnknownPlatform", "could not open the code editing IDE. The operating system is unknown." );
	return false;
#endif

	// Open the solution with the default application
	const FString FullPath = FPaths::Combine(*SolutionFolder, *CodeSolutionFile);
#if PLATFORM_MAC
	if ( IFileManager::Get().DirectoryExists(*FullPath) )
#else
	if ( FPaths::FileExists(FullPath) )
#endif
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication( *FullPath );
		return true;
	}
	else
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("Path"), FText::FromString( FullPath ) );
		OutFailReason = FText::Format( LOCTEXT( "OpenCodeIDE_MissingFile", "Could not edit the code editing IDE. {Path} could not be found." ), Args );
		return false;
	}
}

void GameProjectUtils::GetStarterContentFiles(TArray<FString>& OutFilenames)
{
	FString const SrcFolder = FPaths::StarterContentDir();
	FString const ContentFolder = SrcFolder / TEXT("Content");

	// only copying /Content
	IFileManager::Get().FindFilesRecursive(OutFilenames, *ContentFolder, TEXT("*"), /*Files=*/true, /*Directories=*/false);
}

bool GameProjectUtils::CopyStarterContent(const FString& DestProjectFolder, FText& OutFailReason)
{
	FString const SrcFolder = FPaths::StarterContentDir();

	TArray<FString> FilesToCopy;
	GetStarterContentFiles(FilesToCopy);

	FScopedSlowTask SlowTask(FilesToCopy.Num(), LOCTEXT("CreatingProjectStatus_CopyingFiles", "Copying Files {SrcFilename}..."));
	SlowTask.MakeDialog();

	TArray<FString> CreatedFiles;
	for (FString SrcFilename : FilesToCopy)
	{
		// Update the slow task dialog
		FFormatNamedArguments Args;
		Args.Add(TEXT("SrcFilename"), FText::FromString(FPaths::GetCleanFilename(SrcFilename)));
		SlowTask.EnterProgressFrame(1, FText::Format(LOCTEXT("CreatingProjectStatus_CopyingFile", "Copying File {SrcFilename}..."), Args));

		FString FileRelPath = FPaths::GetPath(SrcFilename);
		FPaths::MakePathRelativeTo(FileRelPath, *SrcFolder);

		// Perform the copy. For file collisions, leave existing file.
		const FString DestFilename = DestProjectFolder + TEXT("/") + FileRelPath + TEXT("/") + FPaths::GetCleanFilename(SrcFilename);
		if (!FPaths::FileExists(DestFilename))
		{
			if (IFileManager::Get().Copy(*DestFilename, *SrcFilename, false) == COPY_OK)
			{
				CreatedFiles.Add(DestFilename);
			}
			else
			{
				FFormatNamedArguments FailArgs;
				FailArgs.Add(TEXT("SrcFilename"), FText::FromString(SrcFilename));
				FailArgs.Add(TEXT("DestFilename"), FText::FromString(DestFilename));
				OutFailReason = FText::Format(LOCTEXT("FailedToCopyFile", "Failed to copy \"{SrcFilename}\" to \"{DestFilename}\"."), FailArgs);
				DeleteCreatedFiles(DestProjectFolder, CreatedFiles);
				return false;
			}
		}
	}

	return true;
}


bool GameProjectUtils::CreateProject(const FProjectInformation& InProjectInfo, FText& OutFailReason)
{
	if ( !IsValidProjectFileForCreation(InProjectInfo.ProjectFilename, OutFailReason) )
	{
		return false;
	}

	FScopedSlowTask SlowTask(0, LOCTEXT( "CreatingProjectStatus", "Creating project..." ));
	SlowTask.MakeDialog();

	bool bProjectCreationSuccessful = false;
	FString TemplateName;
	if ( InProjectInfo.TemplateFile.IsEmpty() )
	{
		bProjectCreationSuccessful = GenerateProjectFromScratch(InProjectInfo, OutFailReason);
		TemplateName = InProjectInfo.bShouldGenerateCode ? TEXT("Basic Code") : TEXT("Blank");
	}
	else
	{
		bProjectCreationSuccessful = CreateProjectFromTemplate(InProjectInfo, OutFailReason);
		TemplateName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
	}

	if( FEngineAnalytics::IsAvailable() )
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Template"), TemplateName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ProjectType"), InProjectInfo.bShouldGenerateCode ? TEXT("C++ Code") : TEXT("Content Only")));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Outcome"), bProjectCreationSuccessful ? TEXT("Successful") : TEXT("Failed")));

		UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EHardwareClass"), true);
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("HardwareClass"), Enum ? Enum->GetEnumName(InProjectInfo.TargetedHardware) : FString()));
		Enum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EGraphicsPreset"), true);
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("GraphicsPreset"), Enum ? Enum->GetEnumName(InProjectInfo.DefaultGraphicsPerformance) : FString()));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("StarterContent"), InProjectInfo.bCopyStarterContent ? TEXT("Yes") : TEXT("No")));

		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.NewProject.ProjectCreated" ), EventAttributes );
	}

	return bProjectCreationSuccessful;
}

void GameProjectUtils::CheckForOutOfDateGameProjectFile()
{
	if ( FPaths::IsProjectFilePathSet() )
	{
		FProjectStatus ProjectStatus;
		if (IProjectManager::Get().QueryStatusForCurrentProject(ProjectStatus))
		{
			if ( ProjectStatus.bRequiresUpdate )
			{
				const FText UpdateProjectText = LOCTEXT("UpdateProjectFilePrompt", "Project file is saved in an older format. Would you like to update it?");
				const FText UpdateProjectConfirmText = LOCTEXT("UpdateProjectFileConfirm", "Update");
				const FText UpdateProjectCancelText = LOCTEXT("UpdateProjectFileCancel", "Not Now");

				FNotificationInfo Info(UpdateProjectText);
				Info.bFireAndForget = false;
				Info.bUseLargeFont = false;
				Info.bUseThrobber = false;
				Info.bUseSuccessFailIcons = false;
				Info.FadeOutDuration = 3.f;
				Info.ButtonDetails.Add(FNotificationButtonInfo(UpdateProjectConfirmText, FText(), FSimpleDelegate::CreateStatic(&GameProjectUtils::OnUpdateProjectConfirm)));
				Info.ButtonDetails.Add(FNotificationButtonInfo(UpdateProjectCancelText, FText(), FSimpleDelegate::CreateStatic(&GameProjectUtils::OnUpdateProjectCancel)));

				if (UpdateGameProjectNotification.IsValid())
				{
					UpdateGameProjectNotification.Pin()->ExpireAndFadeout();
					UpdateGameProjectNotification.Reset();
				}

				UpdateGameProjectNotification = FSlateNotificationManager::Get().AddNotification(Info);

				if (UpdateGameProjectNotification.IsValid())
				{
					UpdateGameProjectNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
				}
			}
		}
	}
}

void GameProjectUtils::CheckAndWarnProjectFilenameValid()
{
	const FString& LoadedProjectFilePath = FPaths::IsProjectFilePathSet() ? FPaths::GetProjectFilePath() : FString();
	if ( !LoadedProjectFilePath.IsEmpty() )
	{
		const FString BaseProjectFile = FPaths::GetBaseFilename(LoadedProjectFilePath);
		if ( BaseProjectFile.Len() > MAX_PROJECT_NAME_LENGTH )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("MaxProjectNameLength"), MAX_PROJECT_NAME_LENGTH );
			const FText WarningReason = FText::Format( LOCTEXT( "WarnProjectNameTooLong", "Project names must not be longer than {MaxProjectNameLength} characters.\nYou might have problems saving or modifying a project with a longer name." ), Args );
			const FText WarningReasonOkText = LOCTEXT("WarningReasonOkText", "Ok");

			FNotificationInfo Info(WarningReason);
			Info.bFireAndForget = false;
			Info.bUseLargeFont = false;
			Info.bUseThrobber = false;
			Info.bUseSuccessFailIcons = false;
			Info.FadeOutDuration = 3.f;
			Info.ButtonDetails.Add(FNotificationButtonInfo(WarningReasonOkText, FText(), FSimpleDelegate::CreateStatic(&GameProjectUtils::OnWarningReasonOk)));

			if (WarningProjectNameNotification.IsValid())
			{
				WarningProjectNameNotification.Pin()->ExpireAndFadeout();
				WarningProjectNameNotification.Reset();
			}

			WarningProjectNameNotification = FSlateNotificationManager::Get().AddNotification(Info);

			if (WarningProjectNameNotification.IsValid())
			{
				WarningProjectNameNotification.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
			}
		}
	}
}

void GameProjectUtils::OnWarningReasonOk()
{
	if ( WarningProjectNameNotification.IsValid() )
	{
		WarningProjectNameNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);
		WarningProjectNameNotification.Pin()->ExpireAndFadeout();
		WarningProjectNameNotification.Reset();
	}
}

bool GameProjectUtils::UpdateGameProject(const FString& ProjectFile, const FString& EngineIdentifier, FText& OutFailReason)
{
	return UpdateGameProjectFile(ProjectFile, EngineIdentifier, NULL, OutFailReason);
}

void GameProjectUtils::OpenAddCodeToProjectDialog(const UClass* InClass, const FString& InInitialPath, const TSharedPtr<SWindow>& InParentWindow, bool bModal, FOnCodeAddedToProject OnCodeAddedToProject, const FString InDefaultClassPrefix, const FString InDefaultClassName )
{
	// If we've been given a class then we only show the second page of the dialog, so we can make the window smaller as that page doesn't have as much content
	const FVector2D WindowSize = (InClass) ? FVector2D(940, 380) : FVector2D(940, 540);

	TSharedRef<SWindow> AddCodeWindow =
		SNew(SWindow)
		.Title(LOCTEXT( "AddCodeWindowHeader", "Add Code"))
		.ClientSize( WindowSize )
		.SizingRule( ESizingRule::FixedSize )
		.SupportsMinimize(false) .SupportsMaximize(false);

	TSharedRef<SNewClassDialog> NewClassDialog = 
		SNew(SNewClassDialog)
		.Class(InClass)
		.InitialPath(InInitialPath)
		.OnCodeAddedToProject( OnCodeAddedToProject )
		.DefaultClassPrefix( InDefaultClassPrefix )
		.DefaultClassName( InDefaultClassName );

	AddCodeWindow->SetContent( NewClassDialog );

	TSharedPtr<SWindow> ParentWindow = InParentWindow;
	if (!ParentWindow.IsValid())
	{
		static const FName MainFrameModuleName = "MainFrame";
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(MainFrameModuleName);
		ParentWindow = MainFrameModule.GetParentWindow();
	}

	if (ParentWindow.IsValid())
	{
		if( bModal )
		{
			FSlateApplication::Get().AddModalWindow(AddCodeWindow, ParentWindow);
		}
		else
		{
			FSlateApplication::Get().AddWindowAsNativeChild(AddCodeWindow, ParentWindow.ToSharedRef());
		}
	}
	else
	{
		if(bModal)
		{
			FSlateApplication::Get().AddModalWindow(AddCodeWindow, nullptr);
		}
		else
		{
			FSlateApplication::Get().AddWindow(AddCodeWindow);
		}

	}
}

bool GameProjectUtils::IsValidClassNameForCreation(const FString& NewClassName, const FModuleContextInfo& ModuleInfo, const TSet<FString>& DisallowedHeaderNames, FText& OutFailReason)
{
	if ( NewClassName.IsEmpty() )
	{
		OutFailReason = LOCTEXT( "NoClassName", "You must specify a class name." );
		return false;
	}

	if ( NewClassName.Contains(TEXT(" ")) )
	{
		OutFailReason = LOCTEXT( "ClassNameContainsSpace", "Your class name may not contain a space." );
		return false;
	}

	if ( !FChar::IsAlpha(NewClassName[0]) )
	{
		OutFailReason = LOCTEXT( "ClassNameMustBeginWithACharacter", "Your class name must begin with an alphabetic character." );
		return false;
	}

	if ( NewClassName.Len() > MAX_CLASS_NAME_LENGTH )
	{
		OutFailReason = FText::Format( LOCTEXT( "ClassNameTooLong", "The class name must not be longer than {0} characters." ), FText::AsNumber(MAX_CLASS_NAME_LENGTH) );
		return false;
	}

	FString IllegalNameCharacters;
	if ( !NameContainsOnlyLegalCharacters(NewClassName, IllegalNameCharacters) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("IllegalNameCharacters"), FText::FromString( IllegalNameCharacters ) );
		OutFailReason = FText::Format( LOCTEXT( "ClassNameContainsIllegalCharacters", "The class name may not contain the following characters: {IllegalNameCharacters}" ), Args );
		return false;
	}

	// Look for a duplicate class in memory
	for ( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
	{
		if ( ClassIt->GetName() == NewClassName )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("NewClassName"), FText::FromString( NewClassName ) );
			OutFailReason = FText::Format( LOCTEXT("ClassNameAlreadyExists", "The name {NewClassName} is already used by another class."), Args );
			return false;
		}
	}

	// Look for a duplicate class on disk in their project
	{
		FString UnusedFoundPath;
		if ( FindSourceFileInProject(NewClassName + ".h", ModuleInfo.ModuleSourcePath, UnusedFoundPath) )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("NewClassName"), FText::FromString( NewClassName ) );
			OutFailReason = FText::Format( LOCTEXT("ClassNameAlreadyExists", "The name {NewClassName} is already used by another class."), Args );
			return false;
		}
	}

	// See if header name clashes with an engine header
	{
		FString UnusedFoundPath;
		if (DisallowedHeaderNames.Contains(NewClassName))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("NewHeaderName"), FText::FromString(NewClassName + ".h"));
			OutFailReason = FText::Format(LOCTEXT("HeaderNameAlreadyExists", "The file {NewHeaderName} already exists elsewhere in the engine."), Args);
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::IsValidBaseClassForCreation(const UClass* InClass, const FModuleContextInfo& InModuleInfo)
{
	auto DoesClassNeedAPIExport = [&InModuleInfo](const FString& InClassModuleName) -> bool
	{
		return InModuleInfo.ModuleName != InClassModuleName;
	};

	return IsValidBaseClassForCreation_Internal(InClass, FDoesClassNeedAPIExportCallback::CreateLambda(DoesClassNeedAPIExport));
}

bool GameProjectUtils::IsValidBaseClassForCreation(const UClass* InClass, const TArray<FModuleContextInfo>& InModuleInfoArray)
{
	auto DoesClassNeedAPIExport = [&InModuleInfoArray](const FString& InClassModuleName) -> bool
	{
		for(const FModuleContextInfo& ModuleInfo : InModuleInfoArray)
		{
			if(ModuleInfo.ModuleName == InClassModuleName)
			{
				return false;
			}
		}
		return true;
	};

	return IsValidBaseClassForCreation_Internal(InClass, FDoesClassNeedAPIExportCallback::CreateLambda(DoesClassNeedAPIExport));
}

bool GameProjectUtils::IsValidBaseClassForCreation_Internal(const UClass* InClass, const FDoesClassNeedAPIExportCallback& InDoesClassNeedAPIExport)
{
	// You may not make native classes based on blueprint generated classes
	const bool bIsBlueprintClass = (InClass->ClassGeneratedBy != nullptr);

	// UObject is special cased to be extensible since it would otherwise not be since it doesn't pass the API check (intrinsic class).
	const bool bIsExplicitlyUObject = (InClass == UObject::StaticClass());

	// You need API if you are not UObject itself, and you're in a module that was validated as needing API export
	const FString ClassModuleName = InClass->GetOutermost()->GetName().RightChop( FString(TEXT("/Script/")).Len() );
	const bool bNeedsAPI = !bIsExplicitlyUObject && InDoesClassNeedAPIExport.Execute(ClassModuleName);

	// You may not make a class that is not DLL exported.
	// MinimalAPI classes aren't compatible with the DLL export macro, but can still be used as a valid base
	const bool bHasAPI = InClass->HasAnyClassFlags(CLASS_RequiredAPI) || InClass->HasAnyClassFlags(CLASS_MinimalAPI);

	// @todo should we support interfaces?
	const bool bIsInterface = InClass->IsChildOf(UInterface::StaticClass());

	return !bIsBlueprintClass && (!bNeedsAPI || bHasAPI) && !bIsInterface;
}

bool GameProjectUtils::AddCodeToProject(const FString& NewClassName, const FString& NewClassPath, const FModuleContextInfo& ModuleInfo, const FNewClassInfo ParentClassInfo, const TSet<FString>& DisallowedHeaderNames, FString& OutHeaderFilePath, FString& OutCppFilePath, FText& OutFailReason)
{
	const bool bAddCodeSuccessful = AddCodeToProject_Internal(NewClassName, NewClassPath, ModuleInfo, ParentClassInfo, DisallowedHeaderNames, OutHeaderFilePath, OutCppFilePath, OutFailReason);

	if( FEngineAnalytics::IsAvailable() )
	{
		const FString ParentClassName = ParentClassInfo.GetClassNameCPP();

		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ParentClass"), ParentClassName.IsEmpty() ? TEXT("None") : ParentClassName));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Outcome"), bAddCodeSuccessful ? TEXT("Successful") : TEXT("Failed")));

		FEngineAnalytics::GetProvider().RecordEvent( TEXT( "Editor.AddCodeToProject.CodeAdded" ), EventAttributes );
	}

	return bAddCodeSuccessful;
}

UTemplateProjectDefs* GameProjectUtils::LoadTemplateDefs(const FString& ProjectDirectory)
{
	UTemplateProjectDefs* TemplateDefs = NULL;

	const FString TemplateDefsIniFilename = ProjectDirectory / TEXT("Config") / GetTemplateDefsFilename();
	if ( FPlatformFileManager::Get().GetPlatformFile().FileExists(*TemplateDefsIniFilename) )
	{
		UClass* ClassToConstruct = UDefaultTemplateProjectDefs::StaticClass();

		// see if template uses a custom project defs object
		FString ClassName;
		const bool bFoundValue = GConfig->GetString(*UTemplateProjectDefs::StaticClass()->GetPathName(), TEXT("TemplateProjectDefsClass"), ClassName, TemplateDefsIniFilename);
		if (bFoundValue && ClassName.Len() > 0)
		{
			UClass* OverrideClass = FindObject<UClass>(ANY_PACKAGE, *ClassName, false);
			if (nullptr != OverrideClass)
			{
				ClassToConstruct = OverrideClass;
			}
			else
			{
				UE_LOG(LogGameProjectGeneration, Error, TEXT("Failed to find template project defs class '%s', using default."), *ClassName);
			}
		}
		TemplateDefs = ConstructObject<UTemplateProjectDefs>(ClassToConstruct);
		TemplateDefs->LoadConfig(UTemplateProjectDefs::StaticClass(), *TemplateDefsIniFilename);
	}

	return TemplateDefs;
}

bool GameProjectUtils::GenerateProjectFromScratch(const FProjectInformation& InProjectInfo, FText& OutFailReason)
{
	FScopedSlowTask SlowTask(5);

	const FString NewProjectFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);
	const FString NewProjectName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);
	TArray<FString> CreatedFiles;

	SlowTask.EnterProgressFrame();

	// Generate config files
	if (!GenerateConfigFiles(InProjectInfo, CreatedFiles, OutFailReason))
	{
		DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
		return false;
	}

	// Make the Content folder
	const FString ContentFolder = NewProjectFolder / TEXT("Content");
	if ( !IFileManager::Get().MakeDirectory(*ContentFolder) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("ContentFolder"), FText::FromString( ContentFolder ) );
		OutFailReason = FText::Format( LOCTEXT("FailedToCreateContentFolder", "Failed to create the content folder {ContentFolder}"), Args );
		DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
		return false;
	}

	SlowTask.EnterProgressFrame();

	TArray<FString> StartupModuleNames;
	if ( InProjectInfo.bShouldGenerateCode )
	{
		FScopedSlowTask LocalScope(2);

		LocalScope.EnterProgressFrame();
		// Generate basic source code files
		if ( !GenerateBasicSourceCode(NewProjectFolder / TEXT("Source"), NewProjectName, NewProjectFolder, StartupModuleNames, CreatedFiles, OutFailReason) )
		{
			DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
			return false;
		}

		LocalScope.EnterProgressFrame();
		// Generate game framework source code files
		if ( !GenerateGameFrameworkSourceCode(NewProjectFolder / TEXT("Source"), NewProjectName, CreatedFiles, OutFailReason) )
		{
			DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	// Generate the project file
	{
		FText LocalFailReason;
		if (IProjectManager::Get().GenerateNewProjectFile(InProjectInfo.ProjectFilename, StartupModuleNames, TEXT(""), LocalFailReason))
		{
			CreatedFiles.Add(InProjectInfo.ProjectFilename);
		}
		else
		{
			OutFailReason = LocalFailReason;
			DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
			return false;
		}

		// Set the engine identifier for it. Do this after saving, so it can be correctly detected as foreign or non-foreign.
		if(!SetEngineAssociationForForeignProject(InProjectInfo.ProjectFilename, OutFailReason))
		{
			DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	if ( InProjectInfo.bShouldGenerateCode )
	{
		// Generate project files
		if ( !GenerateCodeProjectFiles(InProjectInfo.ProjectFilename, OutFailReason) )
		{
			DeleteGeneratedProjectFiles(InProjectInfo.ProjectFilename);
			DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	if (InProjectInfo.bCopyStarterContent)
	{
		// Copy the starter content
		if ( !CopyStarterContent(NewProjectFolder, OutFailReason) )
		{
			DeleteGeneratedProjectFiles(InProjectInfo.ProjectFilename);
			DeleteCreatedFiles(NewProjectFolder, CreatedFiles);
			return false;
		}
	}

	UE_LOG(LogGameProjectGeneration, Log, TEXT("Created new project with %d files (plus project files)"), CreatedFiles.Num());
	return true;
}

bool GameProjectUtils::CreateProjectFromTemplate(const FProjectInformation& InProjectInfo, FText& OutFailReason)
{
	FScopedSlowTask SlowTask(10);

	const FString ProjectName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);
	const FString TemplateName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
	const FString SrcFolder = FPaths::GetPath(InProjectInfo.TemplateFile);
	const FString DestFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);

	if ( !FPlatformFileManager::Get().GetPlatformFile().FileExists(*InProjectInfo.TemplateFile) )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("TemplateFile"), FText::FromString( InProjectInfo.TemplateFile ) );
		OutFailReason = FText::Format( LOCTEXT("InvalidTemplate_MissingProject", "Template project \"{TemplateFile}\" does not exist."), Args );
		return false;
	}

	SlowTask.EnterProgressFrame();

	UTemplateProjectDefs* TemplateDefs = LoadTemplateDefs(SrcFolder);
	if ( TemplateDefs == NULL )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("TemplateFile"), FText::FromString( FPaths::GetBaseFilename(InProjectInfo.TemplateFile) ) );
		Args.Add( TEXT("TemplateDefinesFile"), FText::FromString( GetTemplateDefsFilename() ) );
		OutFailReason = FText::Format( LOCTEXT("InvalidTemplate_MissingDefs", "Template project \"{TemplateFile}\" does not have definitions file: '{TemplateDefinesFile}'."), Args );
		return false;
	}

	SlowTask.EnterProgressFrame();

	// Fix up the replacement strings using the specified project name
	TemplateDefs->FixupStrings(TemplateName, ProjectName);

	// Form a list of all extensions we care about
	TSet<FString> ReplacementsInFilesExtensions;
	for ( auto ReplacementIt = TemplateDefs->ReplacementsInFiles.CreateConstIterator(); ReplacementIt; ++ReplacementIt )
	{
		ReplacementsInFilesExtensions.Append((*ReplacementIt).Extensions);
	}

	// Keep a list of created files so we can delete them if project creation fails
	TArray<FString> CreatedFiles;

	SlowTask.EnterProgressFrame();

	// Discover and copy all files in the src folder to the destination, excluding a few files and folders
	TArray<FString> FilesToCopy;
	TArray<FString> FilesThatNeedContentsReplaced;
	TMap<FString, FString> ClassRenames;
	IFileManager::Get().FindFilesRecursive(FilesToCopy, *SrcFolder, TEXT("*"), /*Files=*/true, /*Directories=*/false);

	SlowTask.EnterProgressFrame();
	{
		// Open a new feedback scope for the loop so we can report how far through the copy we are
		FScopedSlowTask InnerSlowTask(FilesToCopy.Num());
		for ( auto FileIt = FilesToCopy.CreateConstIterator(); FileIt; ++FileIt )
		{
			const FString SrcFilename = (*FileIt);

			// Update the progress
			FFormatNamedArguments Args;
			Args.Add( TEXT("SrcFilename"), FText::FromString( FPaths::GetCleanFilename(SrcFilename) ) );
			InnerSlowTask.EnterProgressFrame(1, FText::Format( LOCTEXT( "CreatingProjectStatus_CopyingFile", "Copying File {SrcFilename}..." ), Args ));

		// Get the file path, relative to the src folder
			const FString SrcFileSubpath = SrcFilename.RightChop(SrcFolder.Len() + 1);

			// Skip any files that were configured to be ignored
			bool bThisFileIsIgnored = false;
			for ( auto IgnoreIt = TemplateDefs->FilesToIgnore.CreateConstIterator(); IgnoreIt; ++IgnoreIt )
			{
				if ( SrcFileSubpath == *IgnoreIt )
				{
					// This file was marked as "ignored"
					bThisFileIsIgnored = true;
					break;
				}
			}

			if ( bThisFileIsIgnored )
			{
				// This file was marked as "ignored"
				continue;
			}

			// Skip any folders that were configured to be ignored
			bool bThisFolderIsIgnored = false;
			for ( auto IgnoreIt = TemplateDefs->FoldersToIgnore.CreateConstIterator(); IgnoreIt; ++IgnoreIt )
			{
				if ( SrcFileSubpath.StartsWith((*IgnoreIt) + TEXT("/") ) )
				{
					// This folder was marked as "ignored"
					bThisFolderIsIgnored = true;
					break;
				}
			}

			if ( bThisFolderIsIgnored )
			{
				// This folder was marked as "ignored"
				continue;
			}

			// Retarget any folders that were chosen to be renamed by choosing a new destination subpath now
			FString DestFileSubpathWithoutFilename = FPaths::GetPath(SrcFileSubpath) + TEXT("/");
			for ( auto RenameIt = TemplateDefs->FolderRenames.CreateConstIterator(); RenameIt; ++RenameIt )
			{
				const FTemplateFolderRename& FolderRename = *RenameIt;
				if ( SrcFileSubpath.StartsWith(FolderRename.From + TEXT("/")) )
				{
					// This was a file in a renamed folder. Retarget to the new location
					DestFileSubpathWithoutFilename = FolderRename.To / DestFileSubpathWithoutFilename.RightChop( FolderRename.From.Len() );
				}
			}

			// Retarget any files that were chosen to have parts of their names replaced here
			FString DestBaseFilename = FPaths::GetBaseFilename(SrcFileSubpath);
			const FString FileExtension = FPaths::GetExtension(SrcFileSubpath);
			for ( auto ReplacementIt = TemplateDefs->FilenameReplacements.CreateConstIterator(); ReplacementIt; ++ReplacementIt )
			{
				const FTemplateReplacement& Replacement = *ReplacementIt;
				if ( Replacement.Extensions.Contains( FileExtension ) )
				{
					// This file matched a filename replacement extension, apply it now
					DestBaseFilename = DestBaseFilename.Replace(*Replacement.From, *Replacement.To, Replacement.bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);
				}
			}

			// Perform the copy
			const FString DestFilename = DestFolder / DestFileSubpathWithoutFilename + DestBaseFilename + TEXT(".") + FileExtension;
			if ( IFileManager::Get().Copy(*DestFilename, *SrcFilename) == COPY_OK )
			{
				CreatedFiles.Add(DestFilename);

				if ( ReplacementsInFilesExtensions.Contains(FileExtension) )
				{
					FilesThatNeedContentsReplaced.Add(DestFilename);
				}

				// Allow project template to extract class renames from this file copy
				if (FPaths::GetBaseFilename(SrcFilename) != FPaths::GetBaseFilename(DestFilename)
					&& TemplateDefs->IsClassRename(DestFilename, SrcFilename, FileExtension))
				{
					// Looks like a UObject file!
					ClassRenames.Add(FPaths::GetBaseFilename(SrcFilename), FPaths::GetBaseFilename(DestFilename));
				}
			}
			else
			{
				FFormatNamedArguments FailArgs;
				FailArgs.Add(TEXT("SrcFilename"), FText::FromString(SrcFilename));
				FailArgs.Add(TEXT("DestFilename"), FText::FromString(DestFilename));
				OutFailReason = FText::Format(LOCTEXT("FailedToCopyFile", "Failed to copy \"{SrcFilename}\" to \"{DestFilename}\"."), FailArgs);
				DeleteCreatedFiles(DestFolder, CreatedFiles);
				return false;
			}
		}
	}

	SlowTask.EnterProgressFrame();
	{
		// Open a new feedback scope for the loop so we can report how far through the process we are
		FScopedSlowTask InnerSlowTask(FilesThatNeedContentsReplaced.Num());

		// Open all files with the specified extensions and replace text
		for ( auto FileIt = FilesThatNeedContentsReplaced.CreateConstIterator(); FileIt; ++FileIt )
		{
			InnerSlowTask.EnterProgressFrame();

			const FString FileToFix = *FileIt;
			bool bSuccessfullyProcessed = false;

			FString FileContents;
			if ( FFileHelper::LoadFileToString(FileContents, *FileToFix) )
			{
				for ( auto ReplacementIt = TemplateDefs->ReplacementsInFiles.CreateConstIterator(); ReplacementIt; ++ReplacementIt )
				{
					const FTemplateReplacement& Replacement = *ReplacementIt;
					if ( Replacement.Extensions.Contains( FPaths::GetExtension(FileToFix) ) )
					{
						FileContents = FileContents.Replace(*Replacement.From, *Replacement.To, Replacement.bCaseSensitive ? ESearchCase::CaseSensitive : ESearchCase::IgnoreCase);
					}
				}

				if ( FFileHelper::SaveStringToFile(FileContents, *FileToFix) )
				{
					bSuccessfullyProcessed = true;
				}
			}

			if ( !bSuccessfullyProcessed )
			{
				FFormatNamedArguments Args;
				Args.Add( TEXT("FileToFix"), FText::FromString( FileToFix ) );
				OutFailReason = FText::Format( LOCTEXT("FailedToFixUpFile", "Failed to process file \"{FileToFix}\"."), Args );
				DeleteCreatedFiles(DestFolder, CreatedFiles);
				return false;
			}
		}
	}

	SlowTask.EnterProgressFrame();

	const FString ProjectConfigPath = DestFolder / TEXT("Config");

	// Write out the hardware class target settings chosen for this project
	{
		const FString DefaultEngineIniFilename = ProjectConfigPath / TEXT("DefaultEngine.ini");

		FString FileContents;
		// Load the existing file - if it doesn't exist we create it
		FFileHelper::LoadFileToString(FileContents, *DefaultEngineIniFilename);

		FileContents += LINE_TERMINATOR;
		FileContents += GetHardwareConfigString(InProjectInfo);

		if ( !WriteOutputFile(DefaultEngineIniFilename, FileContents, OutFailReason) )
		{
			return false;
		}
	}

	// Fixup specific ini values
	TArray<FTemplateConfigValue> ConfigValuesToSet;
	TemplateDefs->AddConfigValues(ConfigValuesToSet, TemplateName, ProjectName, InProjectInfo.bShouldGenerateCode);
	new (ConfigValuesToSet) FTemplateConfigValue(TEXT("DefaultGame.ini"), TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectID"), FGuid::NewGuid().ToString(), /*InShouldReplaceExistingValue=*/true);

	// Add all classname fixups
	for ( auto RenameIt = ClassRenames.CreateConstIterator(); RenameIt; ++RenameIt )
	{
		const FString ClassRedirectString = FString::Printf(TEXT("(OldClassName=\"%s\",NewClassName=\"%s\")"), *RenameIt.Key(), *RenameIt.Value());
		new (ConfigValuesToSet) FTemplateConfigValue(TEXT("DefaultEngine.ini"), TEXT("/Script/Engine.Engine"), TEXT("+ActiveClassRedirects"), *ClassRedirectString, /*InShouldReplaceExistingValue=*/false);
	}

	// Fix all specified config values
	for ( auto ConfigIt = ConfigValuesToSet.CreateConstIterator(); ConfigIt; ++ConfigIt )
	{
		const FTemplateConfigValue& ConfigValue = *ConfigIt;
		const FString IniFilename = ProjectConfigPath / ConfigValue.ConfigFile;
		bool bSuccessfullyProcessed = false;

		TArray<FString> FileLines;
		if ( FFileHelper::LoadANSITextFileToStrings(*IniFilename, &IFileManager::Get(), FileLines) )
		{
			FString FileOutput;
			const FString TargetSection = ConfigValue.ConfigSection;
			FString CurSection;
			bool bFoundTargetKey = false;
			for ( auto LineIt = FileLines.CreateConstIterator(); LineIt; ++LineIt )
			{
				FString Line = *LineIt;
				Line.Trim().TrimTrailing();

				bool bShouldExcludeLineFromOutput = false;

				// If we not yet found the target key parse each line looking for it
				if ( !bFoundTargetKey )
				{
					// Check for an empty line. No work needs to be done on these lines
					if ( Line.Len() == 0 )
					{

					}
					// Comment lines start with ";". Skip these lines entirely.
					else if ( Line.StartsWith(TEXT(";")) )
					{
						
					}
					// If this is a section line, update the section
					else if ( Line.StartsWith(TEXT("[")) )
					{
						// If we are entering a new section and we have not yet found our key in the target section, add it to the end of the section
						if ( CurSection == TargetSection )
						{
							FileOutput += ConfigValue.ConfigKey + TEXT("=") + ConfigValue.ConfigValue + LINE_TERMINATOR + LINE_TERMINATOR;
							bFoundTargetKey = true;
						}

						// Update the current section
						CurSection = Line.Mid(1, Line.Len() - 2);
					}
					// This is possibly an actual key/value pair
					else if ( CurSection == TargetSection )
					{
						// Key value pairs contain an equals sign
						const int32 EqualsIdx = Line.Find(TEXT("="));
						if ( EqualsIdx != INDEX_NONE )
						{
							// Determine the key and see if it is the target key
							const FString Key = Line.Left(EqualsIdx);
							if ( Key == ConfigValue.ConfigKey )
							{
								// Found the target key, add it to the output and skip the current line if the target value is supposed to replace
								FileOutput += ConfigValue.ConfigKey + TEXT("=") + ConfigValue.ConfigValue + LINE_TERMINATOR;
								bShouldExcludeLineFromOutput = ConfigValue.bShouldReplaceExistingValue;
								bFoundTargetKey = true;
							}
						}
					}
				}

				// Unless we replaced the key, add this line to the output
				if ( !bShouldExcludeLineFromOutput )
				{
					FileOutput += Line;
					if ( LineIt.GetIndex() < FileLines.Num() - 1 )
					{
						// Add a line terminator on every line except the last
						FileOutput += LINE_TERMINATOR;
					}
				}
			}

			// If the key did not exist, add it here
			if ( !bFoundTargetKey )
			{
				// If we did not end in the correct section, add the section to the bottom of the file
				if ( CurSection != TargetSection )
				{
					FileOutput += LINE_TERMINATOR;
					FileOutput += LINE_TERMINATOR;
					FileOutput += FString::Printf(TEXT("[%s]"), *TargetSection) + LINE_TERMINATOR;
				}

				// Add the key/value here
				FileOutput += ConfigValue.ConfigKey + TEXT("=") + ConfigValue.ConfigValue + LINE_TERMINATOR;
			}

			if ( FFileHelper::SaveStringToFile(FileOutput, *IniFilename) )
			{
				bSuccessfullyProcessed = true;
			}
		}

		if ( !bSuccessfullyProcessed )
		{
			OutFailReason = LOCTEXT("FailedToFixUpDefaultEngine", "Failed to process file DefaultEngine.ini");
			DeleteCreatedFiles(DestFolder, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	// Generate the project file
	{
		// Load the source project
		FProjectDescriptor Project;
		if(!Project.Load(InProjectInfo.TemplateFile, OutFailReason))
		{
			DeleteCreatedFiles(DestFolder, CreatedFiles);
			return false;
		}

		// Update it to current
		Project.EngineAssociation.Empty();
		Project.EpicSampleNameHash = 0;

		// Fix up module names
		const FString BaseSourceName = FPaths::GetBaseFilename(InProjectInfo.TemplateFile);
		const FString BaseNewName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);
		for ( auto ModuleIt = Project.Modules.CreateIterator(); ModuleIt; ++ModuleIt )
		{
			FModuleDescriptor& ModuleInfo = *ModuleIt;
			ModuleInfo.Name = FName(*ModuleInfo.Name.ToString().Replace(*BaseSourceName, *BaseNewName));
		}

		// Save it to disk
		if(!Project.Save(InProjectInfo.ProjectFilename, OutFailReason))
		{
			DeleteCreatedFiles(DestFolder, CreatedFiles);
			return false;
		}

		// Set the engine identifier if it's a foreign project. Do this after saving, so it can be correctly detected as foreign.
		if(!SetEngineAssociationForForeignProject(InProjectInfo.ProjectFilename, OutFailReason))
		{
			DeleteCreatedFiles(DestFolder, CreatedFiles);
			return false;
		}

		// Add it to the list of created files
		CreatedFiles.Add(InProjectInfo.ProjectFilename);
	}

	SlowTask.EnterProgressFrame();

	// Copy resources
	const FString GameModuleSourcePath = DestFolder / TEXT("Source") / ProjectName;
	if (GenerateGameResourceFiles(GameModuleSourcePath, ProjectName, DestFolder, InProjectInfo.bShouldGenerateCode, CreatedFiles, OutFailReason) == false)
	{
		DeleteCreatedFiles(DestFolder, CreatedFiles);
		return false;
	}

	SlowTask.EnterProgressFrame();
	if ( InProjectInfo.bShouldGenerateCode )
	{
		// Generate project files
		if ( !GenerateCodeProjectFiles(InProjectInfo.ProjectFilename, OutFailReason) )
		{
			DeleteGeneratedProjectFiles(InProjectInfo.ProjectFilename);
			DeleteCreatedFiles(DestFolder, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	if (InProjectInfo.bCopyStarterContent)
	{
		// Copy the starter content
		if ( !CopyStarterContent(DestFolder, OutFailReason) )
		{
			DeleteGeneratedProjectFiles(InProjectInfo.ProjectFilename);
			DeleteCreatedFiles(DestFolder, CreatedFiles);
			return false;
		}
	}

	if (!TemplateDefs->PostGenerateProject(DestFolder, SrcFolder, InProjectInfo.ProjectFilename, InProjectInfo.TemplateFile, InProjectInfo.bShouldGenerateCode, OutFailReason))
	{
		DeleteGeneratedProjectFiles(InProjectInfo.ProjectFilename);
		DeleteCreatedFiles(DestFolder, CreatedFiles);
		return false;
	}
	
	return true;
}

bool GameProjectUtils::SetEngineAssociationForForeignProject(const FString& ProjectFileName, FText& OutFailReason)
{
	if(FUProjectDictionary(FPaths::RootDir()).IsForeignProject(ProjectFileName))
	{
		if(!FDesktopPlatformModule::Get()->SetEngineIdentifierForProject(ProjectFileName, FDesktopPlatformModule::Get()->GetCurrentEngineIdentifier()))
		{
			OutFailReason = LOCTEXT("FailedToSetEngineIdentifier", "Couldn't set engine identifier for project");
			return false;
		}
	}
	return true;
}

FString GameProjectUtils::GetTemplateDefsFilename()
{
	return TEXT("TemplateDefs.ini");
}

bool GameProjectUtils::NameContainsOnlyLegalCharacters(const FString& TestName, FString& OutIllegalCharacters)
{
	bool bContainsIllegalCharacters = false;

	// Only allow alphanumeric characters in the project name
	bool bFoundAlphaNumericChar = false;
	for ( int32 CharIdx = 0 ; CharIdx < TestName.Len() ; ++CharIdx )
	{
		const FString& Char = TestName.Mid( CharIdx, 1 );
		if ( !FChar::IsAlnum(Char[0]) && Char != TEXT("_") )
		{
			if ( !OutIllegalCharacters.Contains( Char ) )
			{
				OutIllegalCharacters += Char;
			}

			bContainsIllegalCharacters = true;
		}
	}

	return !bContainsIllegalCharacters;
}

bool GameProjectUtils::NameContainsUnderscoreAndXB1Installed(const FString& TestName)
{
	bool bContainsIllegalCharacters = false;

	// Only allow alphanumeric characters in the project name
	for ( int32 CharIdx = 0 ; CharIdx < TestName.Len() ; ++CharIdx )
	{
		const FString& Char = TestName.Mid( CharIdx, 1 );
		if ( Char == TEXT("_") )
		{
			const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(TEXT("XboxOne"));
			if (Platform)
			{
				FString NotInstalledDocLink;
				if (Platform->IsSdkInstalled(true, NotInstalledDocLink))
				{
					bContainsIllegalCharacters = true;
				}
			}
		}
	}

	return bContainsIllegalCharacters;
}

bool GameProjectUtils::ProjectFileExists(const FString& ProjectFile)
{
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*ProjectFile);
}

bool GameProjectUtils::AnyProjectFilesExistInFolder(const FString& Path)
{
	TArray<FString> ExistingFiles;
	const FString Wildcard = FString::Printf(TEXT("%s/*.%s"), *Path, *FProjectDescriptor::GetExtension());
	IFileManager::Get().FindFiles(ExistingFiles, *Wildcard, /*Files=*/true, /*Directories=*/false);

	return ExistingFiles.Num() > 0;
}

bool GameProjectUtils::CleanupIsEnabled()
{
	// Clean up files when running Rocket (unless otherwise specified on the command line)
	return FParse::Param(FCommandLine::Get(), TEXT("norocketcleanup")) == false;
}

void GameProjectUtils::DeleteCreatedFiles(const FString& RootFolder, const TArray<FString>& CreatedFiles)
{
	if (CleanupIsEnabled())
	{
		for ( auto FileToDeleteIt = CreatedFiles.CreateConstIterator(); FileToDeleteIt; ++FileToDeleteIt )
		{
			IFileManager::Get().Delete(**FileToDeleteIt);
		}

		// If the project folder is empty after deleting all the files we created, delete the directory as well
		TArray<FString> RemainingFiles;
		IFileManager::Get().FindFilesRecursive(RemainingFiles, *RootFolder, TEXT("*.*"), /*Files=*/true, /*Directories=*/false);
		if ( RemainingFiles.Num() == 0 )
		{
			IFileManager::Get().DeleteDirectory(*RootFolder, /*RequireExists=*/false, /*Tree=*/true);
		}
	}
}

void GameProjectUtils::DeleteGeneratedProjectFiles(const FString& NewProjectFile)
{
	if (CleanupIsEnabled())
	{
		const FString NewProjectFolder = FPaths::GetPath(NewProjectFile);
		const FString NewProjectName = FPaths::GetBaseFilename(NewProjectFile);

		// Since it is hard to tell which files were created from the code project file generation process, just delete the entire ProjectFiles folder.
		const FString IntermediateProjectFileFolder = NewProjectFolder / TEXT("Intermediate") / TEXT("ProjectFiles");
		IFileManager::Get().DeleteDirectory(*IntermediateProjectFileFolder, /*RequireExists=*/false, /*Tree=*/true);

		// Delete the solution file
		const FString SolutionFileName = NewProjectFolder / NewProjectName + TEXT(".sln");
		IFileManager::Get().Delete( *SolutionFileName );
	}
}

void GameProjectUtils::DeleteGeneratedBuildFiles(const FString& NewProjectFolder)
{
	if (CleanupIsEnabled())
	{
		// Since it is hard to tell which files were created from the build process, just delete the entire Binaries and Build folders.
		const FString BinariesFolder = NewProjectFolder / TEXT("Binaries");
		const FString BuildFolder    = NewProjectFolder / TEXT("Intermediate") / TEXT("Build");
		IFileManager::Get().DeleteDirectory(*BinariesFolder, /*RequireExists=*/false, /*Tree=*/true);
		IFileManager::Get().DeleteDirectory(*BuildFolder, /*RequireExists=*/false, /*Tree=*/true);
	}
}

FString GameProjectUtils::GetHardwareConfigString(const FProjectInformation& InProjectInfo)
{
	FString HardwareTargeting;
	
	FString TargetHardwareAsString;
	UEnum::GetValueAsString(TEXT("/Script/HardwareTargeting.HardwareTargetingSettings.EHardwareClass"), InProjectInfo.TargetedHardware, /*out*/ TargetHardwareAsString);

	FString GraphicsPresetAsString;
	UEnum::GetValueAsString(TEXT("/Script/HardwareTargeting.HardwareTargetingSettings.EGraphicsPreset"), InProjectInfo.DefaultGraphicsPerformance, /*out*/ GraphicsPresetAsString);

	HardwareTargeting += TEXT("[/Script/HardwareTargeting.HardwareTargetingSettings]") LINE_TERMINATOR;
	HardwareTargeting += FString::Printf(TEXT("TargetedHardwareClass=%s") LINE_TERMINATOR, *TargetHardwareAsString);
	HardwareTargeting += FString::Printf(TEXT("DefaultGraphicsPerformance=%s") LINE_TERMINATOR, *GraphicsPresetAsString);
	HardwareTargeting += LINE_TERMINATOR;

	return HardwareTargeting;
}

bool GameProjectUtils::GenerateConfigFiles(const FProjectInformation& InProjectInfo, TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	const FString NewProjectFolder = FPaths::GetPath(InProjectInfo.ProjectFilename);
	const FString NewProjectName = FPaths::GetBaseFilename(InProjectInfo.ProjectFilename);

	FString ProjectConfigPath = NewProjectFolder / TEXT("Config");

	// DefaultEngine.ini
	{
		const FString DefaultEngineIniFilename = ProjectConfigPath / TEXT("DefaultEngine.ini");
		FString FileContents;

		FileContents += TEXT("[URL]") LINE_TERMINATOR;

		FileContents += GetHardwareConfigString(InProjectInfo);
		FileContents += LINE_TERMINATOR;
		
		if (InProjectInfo.bCopyStarterContent)
		{
			FString StarterContentContentDir = FPaths::StarterContentDir() + TEXT("Content/");

			TArray<FString> StarterContentMapFiles;
			const FString FileWildcard = FString(TEXT("*")) + FPackageName::GetMapPackageExtension();
		
			FString SpecificEditorStartupMap;
			FString SpecificGameDefaultMap;	
			FString FullEditorStartupMapPath;
			FString FullGameDefaultMapPath;

			// First we check if there are maps specified in the DefaultEngine.ini in our starter content folder			
			const FString StarterContentDefaultEngineIniFilename = FPaths::StarterContentDir() / TEXT("Config/DefaultEngine.ini");
			if (FPaths::FileExists(StarterContentDefaultEngineIniFilename))
			{
				FString StarterFileContents;
				if (FFileHelper::LoadFileToString(StarterFileContents, *StarterContentDefaultEngineIniFilename))
				{
					TArray<FString> StarterIniLines;
					StarterFileContents.ParseIntoArrayLines(&StarterIniLines);
					for (int32 Line = 0; Line < StarterIniLines.Num();Line++)
					{
						FString EachLine = StarterIniLines[Line];
						if (EachLine.StartsWith(TEXT("EditorStartupMap")))
						{
							EachLine.Split("=", nullptr, &SpecificEditorStartupMap);
							FullEditorStartupMapPath = (StarterContentContentDir / SpecificEditorStartupMap) + FPackageName::GetMapPackageExtension();
							FullEditorStartupMapPath = FullEditorStartupMapPath.Replace(TEXT("Game/"), TEXT(""));
						}
						if (EachLine.StartsWith(TEXT("GameDefaultMap")))
						{
							EachLine.Split("=", nullptr, &SpecificGameDefaultMap);
							FullGameDefaultMapPath = (StarterContentContentDir / SpecificEditorStartupMap) + FPackageName::GetMapPackageExtension();
							FullGameDefaultMapPath = FullGameDefaultMapPath.Replace(TEXT("Game/"), TEXT(""));
						}
					}					
				}
			}

			// Look for maps in the content folder. If we don't specify maps for EditorStartup and GameDefault we will use the first we find in here
			IFileManager::Get().FindFilesRecursive(StarterContentMapFiles, *FPaths::StarterContentDir(), *FileWildcard, /*Files=*/true, /*Directories=*/false);			
			FString MapPackagePath;
			if (StarterContentMapFiles.Num() > 0)
			{
				const FString BaseMapFilename = FPaths::GetBaseFilename(StarterContentMapFiles[0]);

				FString MapPathRelToContent = FPaths::GetPath(StarterContentMapFiles[0]);
				FPaths::MakePathRelativeTo(MapPathRelToContent, *StarterContentContentDir);

				MapPackagePath = FString(TEXT("/Game/")) + MapPathRelToContent + TEXT("/") + BaseMapFilename;
			}

			// if either the files we specified don't exist or we didn't specify any, use the first map file we found in the content folder.
			if (SpecificEditorStartupMap.IsEmpty() || FPaths::FileExists(FullEditorStartupMapPath) == false)
			{
				SpecificEditorStartupMap = MapPackagePath;
			}
			if (SpecificGameDefaultMap.IsEmpty() || FPaths::FileExists(FullGameDefaultMapPath) == false)
			{
				SpecificGameDefaultMap = MapPackagePath;
			}
			
			// Write out the settings for startup map and game default map
			FileContents += TEXT("[/Script/EngineSettings.GameMapsSettings]") LINE_TERMINATOR;
			FileContents += FString::Printf(TEXT("EditorStartupMap=%s") LINE_TERMINATOR, *SpecificEditorStartupMap);
			FileContents += FString::Printf(TEXT("GameDefaultMap=%s") LINE_TERMINATOR, *SpecificGameDefaultMap);
			if (InProjectInfo.bShouldGenerateCode)
			{
				FileContents += FString::Printf(TEXT("GlobalDefaultGameMode=\"/Script/%s.%sGameMode\"") LINE_TERMINATOR, *NewProjectName, *NewProjectName);
			}			
		}

		if (WriteOutputFile(DefaultEngineIniFilename, FileContents, OutFailReason))
		{
			OutCreatedFiles.Add(DefaultEngineIniFilename);
		}
		else
		{
			return false;
		}
	}

	// DefaultEditor.ini
	{
		const FString DefaultEditorIniFilename = ProjectConfigPath / TEXT("DefaultEditor.ini");
		FString FileContents;
		FileContents += TEXT("[EditoronlyBP]") LINE_TERMINATOR;
		FileContents += TEXT("bAllowClassAndBlueprintPinMatching=true") LINE_TERMINATOR;
		FileContents += TEXT("bReplaceBlueprintWithClass=true") LINE_TERMINATOR;
		FileContents += TEXT("bDontLoadBlueprintOutsideEditor=true") LINE_TERMINATOR;
		FileContents += TEXT("bBlueprintIsNotBlueprintType=true") LINE_TERMINATOR;

		if (WriteOutputFile(DefaultEditorIniFilename, FileContents, OutFailReason))
		{
			OutCreatedFiles.Add(DefaultEditorIniFilename);
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::GenerateBasicSourceCode(TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	TArray<FString> StartupModuleNames;
	if (GameProjectUtils::GenerateBasicSourceCode(FPaths::GameSourceDir().LeftChop(1), FApp::GetGameName(), FPaths::GameDir(), StartupModuleNames, OutCreatedFiles, OutFailReason))
	{
		GameProjectUtils::UpdateProject(&StartupModuleNames);
		return true;
	}

	return false;
}

bool GameProjectUtils::GenerateBasicSourceCode(const FString& NewProjectSourcePath, const FString& NewProjectName, const FString& NewProjectRoot, TArray<FString>& OutGeneratedStartupModuleNames, TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	const FString GameModulePath = NewProjectSourcePath / NewProjectName;
	const FString EditorName = NewProjectName + TEXT("Editor");

	// MyGame.Build.cs
	{
		const FString NewBuildFilename = GameModulePath / NewProjectName + TEXT(".Build.cs");
		TArray<FString> PublicDependencyModuleNames;
		PublicDependencyModuleNames.Add(TEXT("Core"));
		PublicDependencyModuleNames.Add(TEXT("CoreUObject"));
		PublicDependencyModuleNames.Add(TEXT("Engine"));
		PublicDependencyModuleNames.Add(TEXT("InputCore"));
		TArray<FString> PrivateDependencyModuleNames;
		if ( GenerateGameModuleBuildFile(NewBuildFilename, NewProjectName, PublicDependencyModuleNames, PrivateDependencyModuleNames, OutFailReason) )
		{
			OutGeneratedStartupModuleNames.Add(NewProjectName);
			OutCreatedFiles.Add(NewBuildFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGame resource folder
	if (GenerateGameResourceFiles(GameModulePath, NewProjectName, NewProjectRoot, true, OutCreatedFiles, OutFailReason) == false)
	{
		return false;
	}

	// MyGame.Target.cs
	{
		const FString NewTargetFilename = NewProjectSourcePath / NewProjectName + TEXT(".Target.cs");
		TArray<FString> ExtraModuleNames;
		ExtraModuleNames.Add( NewProjectName );
		if ( GenerateGameModuleTargetFile(NewTargetFilename, NewProjectName, ExtraModuleNames, OutFailReason) )
		{
			OutCreatedFiles.Add(NewTargetFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGameEditor.Target.cs
	{
		const FString NewTargetFilename = NewProjectSourcePath / EditorName + TEXT(".Target.cs");
		// Include the MyGame module...
		TArray<FString> ExtraModuleNames;
		ExtraModuleNames.Add(NewProjectName);
		if ( GenerateEditorModuleTargetFile(NewTargetFilename, EditorName, ExtraModuleNames, OutFailReason) )
		{
			OutCreatedFiles.Add(NewTargetFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGame.h
	{
		const FString NewHeaderFilename = GameModulePath / NewProjectName + TEXT(".h");
		TArray<FString> PublicHeaderIncludes;
		PublicHeaderIncludes.Add(TEXT("Engine.h"));
		if ( GenerateGameModuleHeaderFile(NewHeaderFilename, PublicHeaderIncludes, OutFailReason) )
		{
			OutCreatedFiles.Add(NewHeaderFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGame.cpp
	{
		const FString NewCPPFilename = GameModulePath / NewProjectName + TEXT(".cpp");
		if ( GenerateGameModuleCPPFile(NewCPPFilename, NewProjectName, NewProjectName, OutFailReason) )
		{
			OutCreatedFiles.Add(NewCPPFilename);
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::GenerateGameFrameworkSourceCode(const FString& NewProjectSourcePath, const FString& NewProjectName, TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
	const FString GameModulePath = NewProjectSourcePath / NewProjectName;

	// Used to override the code generation validation since the module we're creating isn't the same as the project we currently have loaded
	FModuleContextInfo NewModuleInfo;
	NewModuleInfo.ModuleName = NewProjectName;
	NewModuleInfo.ModuleType = EHostType::Runtime;
	NewModuleInfo.ModuleSourcePath = FPaths::ConvertRelativePathToFull(GameModulePath / ""); // Ensure trailing /

	// MyGameGameMode.h
	{
		const UClass* BaseClass = AGameMode::StaticClass();
		const FString NewClassName = NewProjectName + BaseClass->GetName();
		const FString NewHeaderFilename = GameModulePath / NewClassName + TEXT(".h");
		FString UnusedSyncLocation;
		if ( GenerateClassHeaderFile(NewHeaderFilename, NewClassName, FNewClassInfo(BaseClass), TArray<FString>(), TEXT(""), TEXT(""), UnusedSyncLocation, NewModuleInfo, false, OutFailReason) )
		{
			OutCreatedFiles.Add(NewHeaderFilename);
		}
		else
		{
			return false;
		}
	}

	// MyGameGameMode.cpp
	{
		const UClass* BaseClass = AGameMode::StaticClass();
		const FString NewClassName = NewProjectName + BaseClass->GetName();
		const FString NewCPPFilename = GameModulePath / NewClassName + TEXT(".cpp");
		
		TArray<FString> PropertyOverrides;
		TArray<FString> AdditionalIncludes;
		FString UnusedSyncLocation;

		if ( GenerateClassCPPFile(NewCPPFilename, NewClassName, FNewClassInfo(BaseClass), AdditionalIncludes, PropertyOverrides, TEXT(""), UnusedSyncLocation, NewModuleInfo, OutFailReason) )
		{
			OutCreatedFiles.Add(NewCPPFilename);
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool GameProjectUtils::BuildCodeProject(const FString& ProjectFilename)
{
	// Build the project while capturing the log output. Passing GWarn to CompileGameProject will allow Slate to display the progress bar.
	FStringOutputDevice OutputLog;
	OutputLog.SetAutoEmitLineTerminator(true);
	GLog->AddOutputDevice(&OutputLog);
	bool bCompileSucceeded = FDesktopPlatformModule::Get()->CompileGameProject(FPaths::RootDir(), ProjectFilename, GWarn);
	GLog->RemoveOutputDevice(&OutputLog);

	// Try to compile the modules
	if(!bCompileSucceeded)
	{
		FText DevEnvName = FSourceCodeNavigation::GetSuggestedSourceCodeIDE( true );

		TArray<FText> CompileFailedButtons;
		int32 OpenIDEButton = CompileFailedButtons.Add(FText::Format(LOCTEXT("CompileFailedOpenIDE", "Open with {0}"), DevEnvName));
		int32 ViewLogButton = CompileFailedButtons.Add(LOCTEXT("CompileFailedViewLog", "View build log"));
		CompileFailedButtons.Add(LOCTEXT("CompileFailedCancel", "Cancel"));

		int32 CompileFailedChoice = SVerbChoiceDialog::ShowModal(LOCTEXT("ProjectUpgradeTitle", "Project Conversion Failed"), FText::Format(LOCTEXT("ProjectUpgradeCompileFailed", "The project failed to compile with this version of the engine. Would you like to open the project in {0}?"), DevEnvName), CompileFailedButtons);
		if(CompileFailedChoice == ViewLogButton)
		{
			CompileFailedButtons.RemoveAt(ViewLogButton);
			CompileFailedChoice = SVerbChoiceDialog::ShowModal(LOCTEXT("ProjectUpgradeTitle", "Project Conversion Failed"), FText::Format(LOCTEXT("ProjectUpgradeCompileFailed", "The project failed to compile with this version of the engine. Build output is as follows:\n\n{0}"), FText::FromString(OutputLog)), CompileFailedButtons);
		}

		FText FailReason;
		if(CompileFailedChoice == OpenIDEButton && !GameProjectUtils::OpenCodeIDE(ProjectFilename, FailReason))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FailReason);
		}
	}
	return bCompileSucceeded;
}

bool GameProjectUtils::GenerateCodeProjectFiles(const FString& ProjectFilename, FText& OutFailReason)
{
	FStringOutputDevice OutputLog;
	OutputLog.SetAutoEmitLineTerminator(true);
	GLog->AddOutputDevice(&OutputLog);
	bool bHaveProjectFiles = FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), ProjectFilename, GWarn);
	GLog->RemoveOutputDevice(&OutputLog);

	if(!bHaveProjectFiles)
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("LogOutput"), FText::FromString(OutputLog) );
		OutFailReason = FText::Format(LOCTEXT("CouldNotGenerateProjectFiles", "Failed to generate project files. Log output:\n{LogOutput}"), Args);
		return false;
	}

	return true;
}

bool GameProjectUtils::IsStarterContentAvailableForNewProjects()
{
	TArray<FString> StarterContentFiles;
	GetStarterContentFiles(StarterContentFiles);

	return (StarterContentFiles.Num() > 0);
}

TArray<FModuleContextInfo> GameProjectUtils::GetCurrentProjectModules()
{
	const FProjectDescriptor* const CurrentProject = IProjectManager::Get().GetCurrentProject();
	check(CurrentProject);

	TArray<FModuleContextInfo> RetModuleInfos;

	if (!GameProjectUtils::ProjectHasCodeFiles() || CurrentProject->Modules.Num() == 0)
	{
		// If this project doesn't currently have any code in it, we need to add a dummy entry for the game
		// so that we can still use the class wizard (this module will be created once we add a class)
		FModuleContextInfo ModuleInfo;
		ModuleInfo.ModuleName = FApp::GetGameName();
		ModuleInfo.ModuleType = EHostType::Runtime;
		ModuleInfo.ModuleSourcePath = FPaths::ConvertRelativePathToFull(FPaths::GameSourceDir() / ModuleInfo.ModuleName / ""); // Ensure trailing /
		RetModuleInfos.Emplace(ModuleInfo);
	}

	// Resolve out the paths for each module and add the cut-down into to our output array
	for (const FModuleDescriptor& ModuleDesc : CurrentProject->Modules)
	{
		FModuleContextInfo ModuleInfo;
		ModuleInfo.ModuleName = ModuleDesc.Name.ToString();
		ModuleInfo.ModuleType = ModuleDesc.Type;

		// Try and find the .Build.cs file for this module within our currently loaded project's Source directory
		FString TmpPath;
		if (!FindSourceFileInProject(ModuleInfo.ModuleName + ".Build.cs", FPaths::GameSourceDir(), TmpPath))
		{
			continue;
		}

		// Chop the .Build.cs file off the end of the path
		ModuleInfo.ModuleSourcePath = FPaths::GetPath(TmpPath);
		ModuleInfo.ModuleSourcePath = FPaths::ConvertRelativePathToFull(ModuleInfo.ModuleSourcePath / ""); // Ensure trailing /

		RetModuleInfos.Emplace(ModuleInfo);
	}

	return RetModuleInfos;
}

bool GameProjectUtils::IsValidSourcePath(const FString& InPath, const FModuleContextInfo& ModuleInfo, FText* const OutFailReason)
{
	const FString AbsoluteInPath = FPaths::ConvertRelativePathToFull(InPath) / ""; // Ensure trailing /

	// Validate the path contains no invalid characters
	if(!FPaths::ValidatePath(AbsoluteInPath, OutFailReason))
	{
		return false;
	}

	if(!AbsoluteInPath.StartsWith(ModuleInfo.ModuleSourcePath))
	{
		if(OutFailReason)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ModuleName"), FText::FromString(ModuleInfo.ModuleName));
			Args.Add(TEXT("RootSourcePath"), FText::FromString(ModuleInfo.ModuleSourcePath));
			*OutFailReason = FText::Format( LOCTEXT("SourcePathInvalidForModule", "All source code for '{ModuleName}' must exist within '{RootSourcePath}'"), Args );
		}
		return false;
	}

	return true;
}

bool GameProjectUtils::CalculateSourcePaths(const FString& InPath, const FModuleContextInfo& ModuleInfo, FString& OutHeaderPath, FString& OutSourcePath, FText* const OutFailReason)
{
	const FString AbsoluteInPath = FPaths::ConvertRelativePathToFull(InPath) / ""; // Ensure trailing /
	OutHeaderPath = AbsoluteInPath;
	OutSourcePath = AbsoluteInPath;

	EClassLocation ClassPathLocation = EClassLocation::UserDefined;
	if(!GetClassLocation(InPath, ModuleInfo, ClassPathLocation, OutFailReason))
	{
		return false;
	}

	const FString RootPath = ModuleInfo.ModuleSourcePath;
	const FString PublicPath = RootPath / "Public" / "";		// Ensure trailing /
	const FString PrivatePath = RootPath / "Private" / "";		// Ensure trailing /
	const FString ClassesPath = RootPath / "Classes" / "";		// Ensure trailing /

	// The root path must exist; we will allow the creation of sub-folders, but not the module root!
	// We ignore this check if the project doesn't already have source code in it, as the module folder won't yet have been created
	const bool bHasCodeFiles = GameProjectUtils::ProjectHasCodeFiles();
	if(!IFileManager::Get().DirectoryExists(*RootPath) && bHasCodeFiles)
	{
		if(OutFailReason)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ModuleSourcePath"), FText::FromString(RootPath));
			*OutFailReason = FText::Format(LOCTEXT("SourcePathMissingModuleRoot", "The specified module path does not exist on disk: {ModuleSourcePath}"), Args);
		}
		return false;
	}

	// The rules for placing header files are as follows:
	// 1) If InPath is the source root, and GetClassLocation has said the class header should be in the Public folder, put it in the Public folder
	// 2) Otherwise, just place the header at InPath (the default set above)
	if(AbsoluteInPath == RootPath)
	{
		OutHeaderPath = (ClassPathLocation == EClassLocation::Public) ? PublicPath : AbsoluteInPath;
	}

	// The rules for placing source files are as follows:
	// 1) If InPath is the source root, and GetClassLocation has said the class header should be in the Public folder, put the source file in the Private folder
	// 2) If InPath is contained within the Public or Classes folder of this module, place it in the equivalent path in the Private folder
	// 3) Otherwise, just place the source file at InPath (the default set above)
	if(AbsoluteInPath == RootPath)
	{
		OutSourcePath = (ClassPathLocation == EClassLocation::Public) ? PrivatePath : AbsoluteInPath;
	}
	else if(ClassPathLocation == EClassLocation::Public)
	{
		OutSourcePath = AbsoluteInPath.Replace(*PublicPath, *PrivatePath);
	}
	else if(ClassPathLocation == EClassLocation::Classes)
	{
		OutSourcePath = AbsoluteInPath.Replace(*ClassesPath, *PrivatePath);
	}

	return !OutHeaderPath.IsEmpty() && !OutSourcePath.IsEmpty();
}

bool GameProjectUtils::GetClassLocation(const FString& InPath, const FModuleContextInfo& ModuleInfo, EClassLocation& OutClassLocation, FText* const OutFailReason)
{
	const FString AbsoluteInPath = FPaths::ConvertRelativePathToFull(InPath) / ""; // Ensure trailing /
	OutClassLocation = EClassLocation::UserDefined;

	if(!IsValidSourcePath(InPath, ModuleInfo, OutFailReason))
	{
		return false;
	}

	const FString RootPath = ModuleInfo.ModuleSourcePath;
	const FString PublicPath = RootPath / "Public" / "";		// Ensure trailing /
	const FString PrivatePath = RootPath / "Private" / "";		// Ensure trailing /
	const FString ClassesPath = RootPath / "Classes" / "";		// Ensure trailing /

	// If either the Public or Private path exists, and we're in the root, force the header/source file to use one of these folders
	const bool bPublicPathExists = IFileManager::Get().DirectoryExists(*PublicPath);
	const bool bPrivatePathExists = IFileManager::Get().DirectoryExists(*PrivatePath);
	const bool bForceInternalPath = AbsoluteInPath == RootPath && (bPublicPathExists || bPrivatePathExists);

	if(AbsoluteInPath == RootPath)
	{
		OutClassLocation = (bPublicPathExists || bForceInternalPath) ? EClassLocation::Public : EClassLocation::UserDefined;
	}
	else if(AbsoluteInPath.StartsWith(PublicPath))
	{
		OutClassLocation = EClassLocation::Public;
	}
	else if(AbsoluteInPath.StartsWith(PrivatePath))
	{
		OutClassLocation = EClassLocation::Private;
	}
	else if(AbsoluteInPath.StartsWith(ClassesPath))
	{
		OutClassLocation = EClassLocation::Classes;
	}
	else
	{
		OutClassLocation = EClassLocation::UserDefined;
	}

	return true;
}

GameProjectUtils::EProjectDuplicateResult GameProjectUtils::DuplicateProjectForUpgrade( const FString& InProjectFile, FString& OutNewProjectFile )
{
	IPlatformFile &PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Get the directory part of the project name
	FString OldDirectoryName = FPaths::GetPath(InProjectFile);
	FPaths::NormalizeDirectoryName(OldDirectoryName);
	FString NewDirectoryName = OldDirectoryName;

	// Strip off any previous version number from the project name
	for(int32 LastSpace; NewDirectoryName.FindLastChar(' ', LastSpace); )
	{
		const TCHAR *End = *NewDirectoryName + LastSpace + 1;
		if(End[0] != '4' || End[1] != '.' || !FChar::IsDigit(End[2]))
		{
			break;
		}

		End += 3;

		while(FChar::IsDigit(*End))
		{
			End++;
		}

		if(*End != 0)
		{
			break;
		}

		NewDirectoryName = NewDirectoryName.Left(LastSpace).TrimTrailing();
	}

	// Append the new version number
	NewDirectoryName += FString::Printf(TEXT(" %s"), *GEngineVersion.ToString(EVersionComponent::Minor));

	// Find a directory name that doesn't exist
	FString BaseDirectoryName = NewDirectoryName;
	for(int32 Idx = 2; IFileManager::Get().DirectoryExists(*NewDirectoryName); Idx++)
	{
		NewDirectoryName = FString::Printf(TEXT("%s - %d"), *BaseDirectoryName, Idx);
	}

	// Find all the root directory names
	TArray<FString> RootDirectoryNames;
	IFileManager::Get().FindFiles(RootDirectoryNames, *(OldDirectoryName / TEXT("*")), false, true);

	// Find all the source directories
	TArray<FString> SourceDirectories;
	SourceDirectories.Add(OldDirectoryName);
	for(int32 Idx = 0; Idx < RootDirectoryNames.Num(); Idx++)
	{
		if(RootDirectoryNames[Idx] != TEXT("Binaries") && RootDirectoryNames[Idx] != TEXT("Intermediate") && RootDirectoryNames[Idx] != TEXT("Saved"))
		{
			FString SourceDirectory = OldDirectoryName / RootDirectoryNames[Idx];
			SourceDirectories.Add(SourceDirectory);
			IFileManager::Get().FindFilesRecursive(SourceDirectories, *SourceDirectory, TEXT("*"), false, true, false);
		}
	}

	// Find all the source files
	TArray<FString> SourceFiles;
	for(int32 Idx = 0; Idx < SourceDirectories.Num(); Idx++)
	{
		TArray<FString> SourceNames;
		IFileManager::Get().FindFiles(SourceNames, *(SourceDirectories[Idx] / TEXT("*")), true, false);

		for(int32 NameIdx = 0; NameIdx < SourceNames.Num(); NameIdx++)
		{
			SourceFiles.Add(SourceDirectories[Idx] / SourceNames[NameIdx]);
		}
	}

	// Copy everything
	bool bCopySucceeded = true;
	bool bUserCanceled = false;
	GWarn->BeginSlowTask(LOCTEXT("CreatingCopyOfProject", "Creating copy of project..."), true, true);
	for(int32 Idx = 0; Idx < SourceDirectories.Num() && bCopySucceeded; Idx++)
	{
		FString TargetDirectory = NewDirectoryName + SourceDirectories[Idx].Mid(OldDirectoryName.Len());
		bUserCanceled = GWarn->ReceivedUserCancel();
		bCopySucceeded = !bUserCanceled && PlatformFile.CreateDirectory(*TargetDirectory);
		GWarn->UpdateProgress(Idx + 1, SourceDirectories.Num() + SourceFiles.Num());
	}
	for(int32 Idx = 0; Idx < SourceFiles.Num() && bCopySucceeded; Idx++)
	{
		FString TargetFile = NewDirectoryName + SourceFiles[Idx].Mid(OldDirectoryName.Len());
		bUserCanceled = GWarn->ReceivedUserCancel();
		bCopySucceeded =  !bUserCanceled && PlatformFile.CopyFile(*TargetFile, *SourceFiles[Idx]);
		GWarn->UpdateProgress(SourceDirectories.Num() + Idx + 1, SourceDirectories.Num() + SourceFiles.Num());
	}
	GWarn->EndSlowTask();

	// Wipe the directory if the user canceled or we couldn't update
	if(!bCopySucceeded)
	{
		PlatformFile.DeleteDirectoryRecursively(*NewDirectoryName);
		if(bUserCanceled)
		{
			return EProjectDuplicateResult::UserCanceled;
		}
		else
		{
			return EProjectDuplicateResult::Failed;
		}
	}

	// Otherwise fixup the output project filename
	OutNewProjectFile = NewDirectoryName / FPaths::GetCleanFilename(InProjectFile);
	return EProjectDuplicateResult::Succeeded;
}

void GameProjectUtils::UpdateSupportedTargetPlatforms(const FName& InPlatformName, const bool bIsSupported)
{
	const FString& ProjectFilename = FPaths::GetProjectFilePath();
	if(!ProjectFilename.IsEmpty())
	{
		// First attempt to check out the file if SCC is enabled
		if(ISourceControlModule::Get().IsEnabled())
		{
			FText UnusedFailReason;
			CheckoutGameProjectFile(ProjectFilename, UnusedFailReason);
		}

		// Second make sure the file is writable
		if(FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ProjectFilename))
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ProjectFilename, false);
		}

		IProjectManager::Get().UpdateSupportedTargetPlatformsForCurrentProject(InPlatformName, bIsSupported);
	}
}

void GameProjectUtils::ClearSupportedTargetPlatforms()
{
	const FString& ProjectFilename = FPaths::GetProjectFilePath();
	if(!ProjectFilename.IsEmpty())
	{
		// First attempt to check out the file if SCC is enabled
		if(ISourceControlModule::Get().IsEnabled())
		{
			FText UnusedFailReason;
			CheckoutGameProjectFile(ProjectFilename, UnusedFailReason);
		}

		// Second make sure the file is writable
		if(FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ProjectFilename))
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ProjectFilename, false);
		}

		IProjectManager::Get().ClearSupportedTargetPlatformsForCurrentProject();
	}
}

bool GameProjectUtils::ReadTemplateFile(const FString& TemplateFileName, FString& OutFileContents, FText& OutFailReason)
{
	const FString FullFileName = FPaths::EngineContentDir() / TEXT("Editor") / TEXT("Templates") / TemplateFileName;
	if ( FFileHelper::LoadFileToString(OutFileContents, *FullFileName) )
	{
		return true;
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("FullFileName"), FText::FromString( FullFileName ) );
	OutFailReason = FText::Format( LOCTEXT("FailedToReadTemplateFile", "Failed to read template file \"{FullFileName}\""), Args );
	return false;
}

bool GameProjectUtils::WriteOutputFile(const FString& OutputFilename, const FString& OutputFileContents, FText& OutFailReason)
{
	if ( FFileHelper::SaveStringToFile(OutputFileContents, *OutputFilename ) )
	{
		return true;
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("OutputFilename"), FText::FromString( OutputFilename ) );
	OutFailReason = FText::Format( LOCTEXT("FailedToWriteOutputFile", "Failed to write output file \"{OutputFilename}\". Perhaps the file is Read-Only?"), Args );
	return false;
}

FString GameProjectUtils::MakeCopyrightLine()
{
	const FString CopyrightNotice = GetDefault<UGeneralProjectSettings>()->CopyrightNotice;
	if (!CopyrightNotice.IsEmpty())
	{
		return FString(TEXT("// ")) + CopyrightNotice;
	}
	else
	{
		return FString();
	}
}

FString GameProjectUtils::MakeCommaDelimitedList(const TArray<FString>& InList, bool bPlaceQuotesAroundEveryElement)
{
	FString ReturnString;

	for ( auto ListIt = InList.CreateConstIterator(); ListIt; ++ListIt )
	{
		FString ElementStr;
		if ( bPlaceQuotesAroundEveryElement )
		{
			ElementStr = FString::Printf( TEXT("\"%s\""), **ListIt);
		}
		else
		{
			ElementStr = *ListIt;
		}

		if ( ReturnString.Len() > 0 )
		{
			// If this is not the first item in the list, prepend with a comma
			ElementStr = FString::Printf(TEXT(", %s"), *ElementStr);
		}

		ReturnString += ElementStr;
	}

	return ReturnString;
}

FString GameProjectUtils::MakeIncludeList(const TArray<FString>& InList)
{
	FString ReturnString;

	for ( auto ListIt = InList.CreateConstIterator(); ListIt; ++ListIt )
	{
		ReturnString += FString::Printf( TEXT("#include \"%s\"") LINE_TERMINATOR, **ListIt);
	}

	return ReturnString;
}

FString GameProjectUtils::DetermineModuleIncludePath(const FModuleContextInfo& ModuleInfo, const FString& FileRelativeTo)
{
	FString ModuleIncludePath;

	if(FindSourceFileInProject(ModuleInfo.ModuleName + ".h", ModuleInfo.ModuleSourcePath, ModuleIncludePath))
	{
		// Work out where the module header is; 
		// if it's Public then we can include it without any path since all Public and Classes folders are on the include path
		// if it's located elsewhere, then we'll need to include it relative to the module source root as we can't guarantee 
		// that other folders are on the include paths
		EClassLocation ModuleLocation;
		if(GetClassLocation(ModuleIncludePath, ModuleInfo, ModuleLocation))
		{
			if(ModuleLocation == EClassLocation::Public || ModuleLocation == EClassLocation::Classes)
			{
				ModuleIncludePath = ModuleInfo.ModuleName + ".h";
			}
			else
			{
				// If the path to our new class is the same as the path to the module, we can include it directly
				const FString ModulePath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(ModuleIncludePath));
				const FString ClassPath = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FileRelativeTo));
				if(ModulePath == ClassPath)
				{
					ModuleIncludePath = ModuleInfo.ModuleName + ".h";
				}
				else
				{
					// Updates ModuleIncludePath internally
					if(!FPaths::MakePathRelativeTo(ModuleIncludePath, *ModuleInfo.ModuleSourcePath))
					{
						// Failed; just assume we can include it without any relative path
						ModuleIncludePath = ModuleInfo.ModuleName + ".h";
					}
				}
			}
		}
		else
		{
			// Failed; just assume we can include it without any relative path
			ModuleIncludePath = ModuleInfo.ModuleName + ".h";
		}
	}
	else
	{
		// This could potentially fail when generating new projects if the module file hasn't yet been created; just assume we can include it without any relative path
		ModuleIncludePath = ModuleInfo.ModuleName + ".h";
	}

	return ModuleIncludePath;
}

/**
 * Generates UObject class constructor definition with property overrides.
 *
 * @param Out String to assign generated constructor to.
 * @param PrefixedClassName Prefixed class name for which we generate the constructor.
 * @param PropertyOverridesStr String with property overrides in the constructor.
 * @param OutFailReason Template read function failure reason.
 *
 * @returns True on success. False otherwise.
 */
bool GenerateConstructorDefinition(FString& Out, const FString& PrefixedClassName, const FString& PropertyOverridesStr, FText& OutFailReason)
{
	FString Template;
	if (!GameProjectUtils::ReadTemplateFile(TEXT("UObjectClassConstructorDefinition.template"), Template, OutFailReason))
	{
		return false;
	}

	Out = Template.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	Out = Out.Replace(TEXT("%PROPERTY_OVERRIDES%"), *PropertyOverridesStr, ESearchCase::CaseSensitive);

	return true;
}

/**
 * Generates UObject class constructor declaration.
 *
 * @param Out String to assign generated constructor to.
 * @param PrefixedClassName Prefixed class name for which we generate the constructor.
 * @param OutFailReason Template read function failure reason.
 *
 * @returns True on success. False otherwise.
 */
bool GenerateConstructorDeclaration(FString& Out, const FString& PrefixedClassName, FText& OutFailReason)
{
	FString Template;
	if (!GameProjectUtils::ReadTemplateFile(TEXT("UObjectClassConstructorDeclaration.template"), Template, OutFailReason))
	{
		return false;
	}

	Out = Template.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);

	return true;
}

bool GameProjectUtils::GenerateClassHeaderFile(const FString& NewHeaderFileName, const FString UnPrefixedClassName, const FNewClassInfo ParentClassInfo, const TArray<FString>& ClassSpecifierList, const FString& ClassProperties, const FString& ClassFunctionDeclarations, FString& OutSyncLocation, const FModuleContextInfo& ModuleInfo, bool bDeclareConstructor, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(ParentClassInfo.GetHeaderTemplateFilename(), Template, OutFailReason) )
	{
		return false;
	}

	const FString ClassPrefix = ParentClassInfo.GetClassPrefixCPP();
	const FString PrefixedClassName = ClassPrefix + UnPrefixedClassName;
	const FString PrefixedBaseClassName = ClassPrefix + ParentClassInfo.GetClassNameCPP();

	FString BaseClassIncludeDirective;
	FString BaseClassIncludePath;
	if(ParentClassInfo.GetIncludePath(BaseClassIncludePath))
	{
		BaseClassIncludeDirective = FString::Printf(LINE_TERMINATOR TEXT("#include \"%s\""), *BaseClassIncludePath);
	}

	FString ModuleAPIMacro;
	{
		EClassLocation ClassPathLocation = EClassLocation::UserDefined;
		if ( GetClassLocation(NewHeaderFileName, ModuleInfo, ClassPathLocation) )
		{
			// If this class isn't Private, make sure and include the API macro so it can be linked within other modules
			if ( ClassPathLocation != EClassLocation::Private )
			{
				ModuleAPIMacro = ModuleInfo.ModuleName.ToUpper() + "_API "; // include a trailing space for the template formatting
			}
		}
	}

	FString EventualConstructorDeclaration;
	if (bDeclareConstructor)
	{
		if (!GenerateConstructorDeclaration(EventualConstructorDeclaration, PrefixedClassName, OutFailReason))
		{
			return false;
		}
	}

	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UNPREFIXED_CLASS_NAME%"), *UnPrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_MODULE_API_MACRO%"), *ModuleAPIMacro, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UCLASS_SPECIFIER_LIST%"), *MakeCommaDelimitedList(ClassSpecifierList, false), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_BASE_CLASS_NAME%"), *PrefixedBaseClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EVENTUAL_CONSTRUCTOR_DECLARATION%"), *EventualConstructorDeclaration, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_PROPERTIES%"), *ClassProperties, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%CLASS_FUNCTION_DECLARATIONS%"), *ClassFunctionDeclarations, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%BASE_CLASS_INCLUDE_DIRECTIVE%"), *BaseClassIncludeDirective, ESearchCase::CaseSensitive);

	HarvestCursorSyncLocation( FinalOutput, OutSyncLocation );

	return WriteOutputFile(NewHeaderFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateClassCPPFile(const FString& NewCPPFileName, const FString UnPrefixedClassName, const FNewClassInfo ParentClassInfo, const TArray<FString>& AdditionalIncludes, const TArray<FString>& PropertyOverrides, const FString& AdditionalMemberDefinitions, FString& OutSyncLocation, const FModuleContextInfo& ModuleInfo, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(ParentClassInfo.GetSourceTemplateFilename(), Template, OutFailReason) )
	{
		return false;
	}

	const FString ClassPrefix = ParentClassInfo.GetClassPrefixCPP();
	const FString PrefixedClassName = ClassPrefix + UnPrefixedClassName;
	const FString PrefixedBaseClassName = ClassPrefix + ParentClassInfo.GetClassNameCPP();

	EClassLocation ClassPathLocation = EClassLocation::UserDefined;
	if ( !GetClassLocation(NewCPPFileName, ModuleInfo, ClassPathLocation, &OutFailReason) )
	{
		return false;
	}

	FString AdditionalIncludesStr;
	for (int32 IncludeIdx = 0; IncludeIdx < AdditionalIncludes.Num(); ++IncludeIdx)
	{
		if (IncludeIdx > 0)
		{
			AdditionalIncludesStr += LINE_TERMINATOR;
		}

		AdditionalIncludesStr += FString::Printf(TEXT("#include \"%s\""), *AdditionalIncludes[IncludeIdx]);
	}

	FString PropertyOverridesStr;
	for ( int32 OverrideIdx = 0; OverrideIdx < PropertyOverrides.Num(); ++OverrideIdx )
	{
		if ( OverrideIdx > 0 )
		{
			PropertyOverridesStr += LINE_TERMINATOR;
		}

		PropertyOverridesStr += TEXT("\t");
		PropertyOverridesStr += *PropertyOverrides[OverrideIdx];
	}

	// Calculate the correct include path for the module header
	const FString ModuleIncludePath = DetermineModuleIncludePath(ModuleInfo, NewCPPFileName);


	FString EventualConstructorDefinition;
	if (PropertyOverrides.Num() != 0)
	{
		if (!GenerateConstructorDefinition(EventualConstructorDefinition, PrefixedClassName, PropertyOverridesStr, OutFailReason))
		{
			return false;
		}
	}

	// Not all of these will exist in every class template
	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%UNPREFIXED_CLASS_NAME%"), *UnPrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleInfo.ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_INCLUDE_PATH%"), *ModuleIncludePath, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PREFIXED_CLASS_NAME%"), *PrefixedClassName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EVENTUAL_CONSTRUCTOR_DEFINITION%"), *EventualConstructorDefinition, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%ADDITIONAL_MEMBER_DEFINITIONS%"), *AdditionalMemberDefinitions, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%ADDITIONAL_INCLUDE_DIRECTIVES%"), *AdditionalIncludesStr, ESearchCase::CaseSensitive);

	HarvestCursorSyncLocation( FinalOutput, OutSyncLocation );

	return WriteOutputFile(NewCPPFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleBuildFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& PublicDependencyModuleNames, const TArray<FString>& PrivateDependencyModuleNames, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("GameModule.Build.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PublicDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PRIVATE_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PrivateDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleTargetFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& ExtraModuleNames, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("Stub.Target.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EXTRA_MODULE_NAMES%"), *MakeCommaDelimitedList(ExtraModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%TARGET_TYPE%"), TEXT("Game"), ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameResourceFiles(const FString& NewResourceFolderName, const FString& GameName, const FString& GameRoot, bool bShouldGenerateCode, TArray<FString>& OutCreatedFiles, FText& OutFailReason)
{
#if PLATFORM_WINDOWS
	// Copy the icon if it doesn't already exist. If we're upgrading a content-only project to code, it will already have one unless it was created before content-only project icons were supported.
	FString IconFileName = GameRoot / TEXT("Build/Windows/Application.ico");
	if(!FPaths::FileExists(IconFileName))
	{
		if(!SourceControlHelpers::CopyFileUnderSourceControl(IconFileName, FPaths::EngineContentDir() / TEXT("Editor/Templates/Resources/Windows/_GAME_NAME_.ico"), LOCTEXT("IconFileDescription", "icon"), OutFailReason))
		{
			return false;
		}
		OutCreatedFiles.Add(IconFileName);
	}
	
	// Generate a RC script if it's a code project
	if(bShouldGenerateCode)
	{
		FString OutputFilename = NewResourceFolderName / FString::Printf(TEXT("Resources/Windows/%s.rc"), *GameName);

		FString TemplateText;
		if (!ReadTemplateFile(TEXT("Resources/Windows/_GAME_NAME_.rc"), TemplateText, OutFailReason))
		{
			return false;
		}

		FString RelativeIconPath = IconFileName;
		FPaths::MakePathRelativeTo(RelativeIconPath, *OutputFilename);
		TemplateText = TemplateText.Replace(TEXT("%ICON_PATH%"), *RelativeIconPath, ESearchCase::CaseSensitive);
		TemplateText = TemplateText.Replace(TEXT("%GAME_NAME%"), *GameName, ESearchCase::CaseSensitive);

		struct Local
		{
			static bool WriteFile(const FString& InDestFile, const FText& InFileDescription, FText& OutFailureReason, FString* InFileContents, TArray<FString>* OutCreatedFileList)
			{
				if (WriteOutputFile(InDestFile, *InFileContents, OutFailureReason))
				{
					OutCreatedFileList->Add(InDestFile);
					return true;
				}

				return false;
			}
		};

		if(!SourceControlHelpers::CheckoutOrMarkForAdd(OutputFilename, LOCTEXT("ResourceFileDescription", "resource"), FOnPostCheckOut::CreateStatic(&Local::WriteFile, &TemplateText, &OutCreatedFiles), OutFailReason))
		{
			return false;
		}
	}
#elif PLATFORM_MAC
	//@todo MAC: Implement MAC version of these files...
#endif

	return true;
}

bool GameProjectUtils::GenerateEditorModuleBuildFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& PublicDependencyModuleNames, const TArray<FString>& PrivateDependencyModuleNames, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("EditorModule.Build.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PublicDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PRIVATE_DEPENDENCY_MODULE_NAMES%"), *MakeCommaDelimitedList(PrivateDependencyModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateEditorModuleTargetFile(const FString& NewBuildFileName, const FString& ModuleName, const TArray<FString>& ExtraModuleNames, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("Stub.Target.cs.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%EXTRA_MODULE_NAMES%"), *MakeCommaDelimitedList(ExtraModuleNames), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%TARGET_TYPE%"), TEXT("Editor"), ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleCPPFile(const FString& NewBuildFileName, const FString& ModuleName, const FString& GameName, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("GameModule.cpp.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%MODULE_NAME%"), *ModuleName, ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%GAME_NAME%"), *GameName, ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

bool GameProjectUtils::GenerateGameModuleHeaderFile(const FString& NewBuildFileName, const TArray<FString>& PublicHeaderIncludes, FText& OutFailReason)
{
	FString Template;
	if ( !ReadTemplateFile(TEXT("GameModule.h.template"), Template, OutFailReason) )
	{
		return false;
	}

	FString FinalOutput = Template.Replace(TEXT("%COPYRIGHT_LINE%"), *MakeCopyrightLine(), ESearchCase::CaseSensitive);
	FinalOutput = FinalOutput.Replace(TEXT("%PUBLIC_HEADER_INCLUDES%"), *MakeIncludeList(PublicHeaderIncludes), ESearchCase::CaseSensitive);

	return WriteOutputFile(NewBuildFileName, FinalOutput, OutFailReason);
}

void GameProjectUtils::OnUpdateProjectConfirm()
{
	UpdateProject(NULL);
}

void GameProjectUtils::UpdateProject(const TArray<FString>* StartupModuleNames)
{
	const FString& ProjectFilename = FPaths::GetProjectFilePath();
	const FString& ShortFilename = FPaths::GetCleanFilename(ProjectFilename);
	FText FailReason;
	FText UpdateMessage;
	SNotificationItem::ECompletionState NewCompletionState;
	if ( UpdateGameProjectFile(ProjectFilename, FDesktopPlatformModule::Get()->GetCurrentEngineIdentifier(), StartupModuleNames, FailReason) )
	{
		// The project was updated successfully.
		FFormatNamedArguments Args;
		Args.Add( TEXT("ShortFilename"), FText::FromString( ShortFilename ) );
		UpdateMessage = FText::Format( LOCTEXT("ProjectFileUpdateComplete", "{ShortFilename} was successfully updated."), Args );
		NewCompletionState = SNotificationItem::CS_Success;
	}
	else
	{
		// The user chose to update, but the update failed. Notify the user.
		FFormatNamedArguments Args;
		Args.Add( TEXT("ShortFilename"), FText::FromString( ShortFilename ) );
		Args.Add( TEXT("FailReason"), FailReason );
		UpdateMessage = FText::Format( LOCTEXT("ProjectFileUpdateFailed", "{ShortFilename} failed to update. {FailReason}"), Args );
		NewCompletionState = SNotificationItem::CS_Fail;
	}

	if ( UpdateGameProjectNotification.IsValid() )
	{
		UpdateGameProjectNotification.Pin()->SetCompletionState(NewCompletionState);
		UpdateGameProjectNotification.Pin()->SetText(UpdateMessage);
		UpdateGameProjectNotification.Pin()->ExpireAndFadeout();
		UpdateGameProjectNotification.Reset();
	}
}

void GameProjectUtils::OnUpdateProjectCancel()
{
	if ( UpdateGameProjectNotification.IsValid() )
	{
		UpdateGameProjectNotification.Pin()->SetCompletionState(SNotificationItem::CS_None);
		UpdateGameProjectNotification.Pin()->ExpireAndFadeout();
		UpdateGameProjectNotification.Reset();
	}
}

void GameProjectUtils::TryMakeProjectFileWriteable(const FString& ProjectFile)
{
	// First attempt to check out the file if SCC is enabled
	if ( ISourceControlModule::Get().IsEnabled() )
	{
		FText FailReason;
		GameProjectUtils::CheckoutGameProjectFile(ProjectFile, FailReason);
	}

	// Check if it's writable
	if(FPlatformFileManager::Get().GetPlatformFile().IsReadOnly(*ProjectFile))
	{
		FText ShouldMakeProjectWriteable = LOCTEXT("ShouldMakeProjectWriteable_Message", "'{ProjectFilename}' is read-only and cannot be updated. Would you like to make it writeable?");

		FFormatNamedArguments Arguments;
		Arguments.Add( TEXT("ProjectFilename"), FText::FromString(ProjectFile));

		if(FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(ShouldMakeProjectWriteable, Arguments)) == EAppReturnType::Yes)
		{
			FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ProjectFile, false);
		}
	}
}

bool GameProjectUtils::UpdateGameProjectFile(const FString& ProjectFile, const FString& EngineIdentifier, const TArray<FString>* StartupModuleNames, FText& OutFailReason)
{
	// Make sure we can write to the project file
	TryMakeProjectFileWriteable(ProjectFile);

	// Load the descriptor
	FProjectDescriptor Descriptor;
	if(Descriptor.Load(ProjectFile, OutFailReason))
	{
		// Freshen version information
		Descriptor.EngineAssociation = EngineIdentifier;

		// Replace the modules names, if specified
		if(StartupModuleNames != NULL)
		{
			Descriptor.Modules.Empty();
			for(int32 Idx = 0; Idx < StartupModuleNames->Num(); Idx++)
			{
				Descriptor.Modules.Add(FModuleDescriptor(*(*StartupModuleNames)[Idx]));
			}
		}

		// Update file on disk
		return Descriptor.Save(ProjectFile, OutFailReason);
	}
	return false;
}

bool GameProjectUtils::CheckoutGameProjectFile(const FString& ProjectFilename, FText& OutFailReason)
{
	if ( !ensure(ProjectFilename.Len()) )
	{
		OutFailReason = LOCTEXT("NoProjectFilename", "The project filename was not specified.");
		return false;
	}

	if ( !ISourceControlModule::Get().IsEnabled() )
	{
		OutFailReason = LOCTEXT("SCCDisabled", "Source control is not enabled. Enable source control in the preferences menu.");
		return false;
	}

	FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(ProjectFilename);
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(AbsoluteFilename, EStateCacheUsage::ForceUpdate);
	TArray<FString> FilesToBeCheckedOut;
	FilesToBeCheckedOut.Add(AbsoluteFilename);

	bool bSuccessfullyCheckedOut = false;
	OutFailReason = LOCTEXT("SCCStateInvalid", "Could not determine source control state.");

	if(SourceControlState.IsValid())
	{
		if(SourceControlState->IsCheckedOut() || SourceControlState->IsAdded() || !SourceControlState->IsSourceControlled())
		{
			// Already checked out or opened for add... or not in the depot at all
			bSuccessfullyCheckedOut = true;
		}
		else if(SourceControlState->CanCheckout() || SourceControlState->IsCheckedOutOther())
		{
			bSuccessfullyCheckedOut = (SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut) == ECommandResult::Succeeded);
			if (!bSuccessfullyCheckedOut)
			{
				OutFailReason = LOCTEXT("SCCCheckoutFailed", "Failed to check out the project file.");
			}
		}
		else if(!SourceControlState->IsCurrent())
		{
			OutFailReason = LOCTEXT("SCCNotCurrent", "The project file is not at head revision.");
		}
	}

	return bSuccessfullyCheckedOut;
}

FString GameProjectUtils::GetDefaultProjectTemplateFilename()
{
	return TEXT("");
}

void GameProjectUtils::GetProjectCodeFilenames(TArray<FString>& OutProjectCodeFilenames)
{
	IFileManager::Get().FindFilesRecursive(OutProjectCodeFilenames, *FPaths::GameSourceDir(), TEXT("*.h"), true, false, false);
	IFileManager::Get().FindFilesRecursive(OutProjectCodeFilenames, *FPaths::GameSourceDir(), TEXT("*.cpp"), true, false, false);
}

int32 GameProjectUtils::GetProjectCodeFileCount()
{
	TArray<FString> Filenames;
	GetProjectCodeFilenames(Filenames);
	return Filenames.Num();
}

void GameProjectUtils::GetProjectSourceDirectoryInfo(int32& OutNumCodeFiles, int64& OutDirectorySize)
{
	TArray<FString> Filenames;
	GetProjectCodeFilenames(Filenames);
	OutNumCodeFiles = Filenames.Num();

	OutDirectorySize = 0;
	for (const auto& filename : Filenames)
	{
		OutDirectorySize += IFileManager::Get().FileSize(*filename);
	}
}

bool GameProjectUtils::ProjectHasCodeFiles()
{
	return GameProjectUtils::GetProjectCodeFileCount() > 0;
}

bool GameProjectUtils::AddCodeToProject_Internal(const FString& NewClassName, const FString& NewClassPath, const FModuleContextInfo& ModuleInfo, const FNewClassInfo ParentClassInfo, const TSet<FString>& DisallowedHeaderNames, FString& OutHeaderFilePath, FString& OutCppFilePath, FText& OutFailReason)
{
	if ( !ParentClassInfo.IsSet() )
	{
		OutFailReason = LOCTEXT("NoParentClass", "You must specify a parent class");
		return false;
	}

	const FString CleanClassName = ParentClassInfo.GetCleanClassName(NewClassName);
	const FString FinalClassName = ParentClassInfo.GetFinalClassName(NewClassName);

	if (!IsValidClassNameForCreation(FinalClassName, ModuleInfo, DisallowedHeaderNames, OutFailReason))
	{
		return false;
	}

	if ( !FApp::HasGameName() )
	{
		OutFailReason = LOCTEXT("AddCodeToProject_NoGameName", "You can not add code because you have not loaded a project.");
		return false;
	}

	FString NewHeaderPath;
	FString NewCppPath;
	if ( !CalculateSourcePaths(NewClassPath, ModuleInfo, NewHeaderPath, NewCppPath, &OutFailReason) )
	{
		return false;
	}

	FScopedSlowTask SlowTask( 6, LOCTEXT( "AddingCodeToProject", "Adding code to project..." ) );
	SlowTask.MakeDialog();

	SlowTask.EnterProgressFrame();
	
	// If the project does not already contain code, add the primary game module
	TArray<FString> CreatedFiles;
	bool bDidNotHaveAnyCodeFiles = !ProjectHasCodeFiles();
	if (bDidNotHaveAnyCodeFiles)
	{
		// We always add the basic source code to the root directory, not the potential sub-directory provided by NewClassPath
		const FString SourceDir = FPaths::GameSourceDir().LeftChop(1); // Trim the trailing /

		// Assuming the game name is the same as the primary game module name
		const FString GameModuleName = FApp::GetGameName();

		TArray<FString> StartupModuleNames;
		if ( GenerateBasicSourceCode(SourceDir, GameModuleName, FPaths::GameDir(), StartupModuleNames, CreatedFiles, OutFailReason) )
		{
			UpdateProject(&StartupModuleNames);
		}
		else
		{
			DeleteCreatedFiles(SourceDir, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	// Class Header File
	const FString NewHeaderFilename = NewHeaderPath / ParentClassInfo.GetHeaderFilename(NewClassName);
	{
		FString UnusedSyncLocation;
		if ( GenerateClassHeaderFile(NewHeaderFilename, CleanClassName, ParentClassInfo, TArray<FString>(), TEXT(""), TEXT(""), UnusedSyncLocation, ModuleInfo, false, OutFailReason) )
		{
			CreatedFiles.Add(NewHeaderFilename);
		}
		else
		{
			DeleteCreatedFiles(NewHeaderPath, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	// Class CPP file
	const FString NewCppFilename = NewCppPath / ParentClassInfo.GetSourceFilename(NewClassName);
	{
		FString UnusedSyncLocation;
		if ( GenerateClassCPPFile(NewCppFilename, CleanClassName, ParentClassInfo, TArray<FString>(), TArray<FString>(), TEXT(""), UnusedSyncLocation, ModuleInfo, OutFailReason) )
		{
			CreatedFiles.Add(NewCppFilename);
		}
		else
		{
			DeleteCreatedFiles(NewCppPath, CreatedFiles);
			return false;
		}
	}

	SlowTask.EnterProgressFrame();

	// Generate project files if we happen to be using a project file.
	if ( !FDesktopPlatformModule::Get()->GenerateProjectFiles(FPaths::RootDir(), FPaths::GetProjectFilePath(), GWarn) )
	{
		OutFailReason = LOCTEXT("FailedToGenerateProjectFiles", "Failed to generate project files.");
		return false;
	}

	SlowTask.EnterProgressFrame();

	// Mark the files for add in SCC
	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
	if ( ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable() )
	{
		TArray<FString> FilesToCheckOut;
		for ( auto FileIt = CreatedFiles.CreateConstIterator(); FileIt; ++FileIt )
		{
			FilesToCheckOut.Add( IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(**FileIt) );
		}

		SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToCheckOut);
	}

	SlowTask.EnterProgressFrame( 1.0f, LOCTEXT("CompilingCPlusPlusCode", "Compiling new C++ code.  Please wait..."));

	OutHeaderFilePath = NewHeaderFilename;
	OutCppFilePath = NewCppFilename;

	if (bDidNotHaveAnyCodeFiles)
	{
		// This is the first time we add code to this project so compile its game DLL
		const FString GameModuleName = FApp::GetGameName();
		check(ModuleInfo.ModuleName == GameModuleName);

		IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
		const bool bReloadAfterCompiling = true;
		const bool bForceCodeProject = true;
		const bool bFailIfGeneratedCodeChanges = false;
		if (!HotReloadSupport.RecompileModule(*GameModuleName, bReloadAfterCompiling, *GWarn, bFailIfGeneratedCodeChanges, bForceCodeProject))
		{
			OutFailReason = LOCTEXT("FailedToCompileNewGameModule", "Failed to compile newly created game module.");
			return false;
		}

		// Notify that we've created a brand new module
		FSourceCodeNavigation::AccessOnNewModuleAdded().Broadcast(*GameModuleName);
	}
	else if (GEditor->AccessEditorUserSettings().bAutomaticallyHotReloadNewClasses)
	{
		FModuleStatus ModuleStatus;
		const FName ModuleFName = *ModuleInfo.ModuleName;
		if (ensure(FModuleManager::Get().QueryModule(ModuleFName, ModuleStatus)))
		{
			// Compile the module that the class was added to so that the newly added class with appear in the Content Browser
			TArray<UPackage*> PackagesToRebind;
			if (ModuleStatus.bIsLoaded)
			{
				const bool bIsHotReloadable = FModuleManager::Get().DoesLoadedModuleHaveUObjects(ModuleFName);
				if (bIsHotReloadable)
				{
					// Is there a UPackage with the same name as this module?
					const FString PotentialPackageName = FString(TEXT("/Script/")) + ModuleInfo.ModuleName;
					UPackage* Package = FindPackage(nullptr, *PotentialPackageName);
					if (Package)
					{
						PackagesToRebind.Add(Package);
					}
				}
			}

			IHotReloadInterface& HotReloadSupport = FModuleManager::LoadModuleChecked<IHotReloadInterface>("HotReload");
			if (PackagesToRebind.Num() > 0)
			{
				// Perform a hot reload
				const bool bWaitForCompletion = true;			
				ECompilationResult::Type CompilationResult = HotReloadSupport.RebindPackages( PackagesToRebind, TArray<FName>(), bWaitForCompletion, *GWarn );
				if( CompilationResult != ECompilationResult::Succeeded && CompilationResult != ECompilationResult::UpToDate )
				{
					OutFailReason = FText::Format(LOCTEXT("FailedToHotReloadModuleFmt", "Failed to automatically hot reload the '{0}' module."), FText::FromString(ModuleInfo.ModuleName));
					return false;
				}
			}
			else
			{
				// Perform a regular unload, then reload
				const bool bReloadAfterRecompile = true;
				const bool bForceCodeProject = false;
				const bool bFailIfGeneratedCodeChanges = true;
				if (!HotReloadSupport.RecompileModule(ModuleFName, bReloadAfterRecompile, *GWarn, bFailIfGeneratedCodeChanges, bForceCodeProject))
				{
					OutFailReason = FText::Format(LOCTEXT("FailedToCompileModuleFmt", "Failed to automatically compile the '{0}' module."), FText::FromString(ModuleInfo.ModuleName));
					return false;
				}
			}
		}
	}

	return true;
}

bool GameProjectUtils::FindSourceFileInProject(const FString& InFilename, const FString& InSearchPath, FString& OutPath)
{
	TArray<FString> Filenames;
	const FString FilenameWidcard = TEXT("*") + InFilename;
	IFileManager::Get().FindFilesRecursive(Filenames, *InSearchPath, *FilenameWidcard, true, false, false);
	
	if(Filenames.Num())
	{
		// Assume it's the first match (we should really only find a single file with a given name within a project anyway)
		OutPath = Filenames[0];
		return true;
	}

	return false;
}


void GameProjectUtils::HarvestCursorSyncLocation( FString& FinalOutput, FString& OutSyncLocation )
{
	OutSyncLocation.Empty();

	// Determine the cursor focus location if this file will by synced after creation
	TArray<FString> Lines;
	FinalOutput.ParseIntoArray( &Lines, TEXT( "\n" ), false );
	for( int32 LineIdx = 0; LineIdx < Lines.Num(); ++LineIdx )
	{
		const FString& Line = Lines[ LineIdx ];
		int32 CharLoc = Line.Find( TEXT( "%CURSORFOCUSLOCATION%" ) );
		if( CharLoc != INDEX_NONE )
		{
			// Found the sync marker
			OutSyncLocation = FString::Printf( TEXT( "%d:%d" ), LineIdx + 1, CharLoc + 1 );
			break;
		}
	}

	// If we did not find the sync location, just sync to the top of the file
	if( OutSyncLocation.IsEmpty() )
	{
		OutSyncLocation = TEXT( "1:1" );
	}

	// Now remove the cursor focus marker
	FinalOutput = FinalOutput.Replace(TEXT("%CURSORFOCUSLOCATION%"), TEXT(""), ESearchCase::CaseSensitive);
}

#undef LOCTEXT_NAMESPACE
