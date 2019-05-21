#pragma once

#include "ILauncherServicesModule.h"
#include "ILauncher.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "AndroidRuntimeSettings.h"
#include "OVR_Plugin.h"

enum EBuildStage
{
	UNDEFINED_STAGE,
	COOK_IN_EDITOR_STAGE,
	COOK_STAGE,
	LAUNCH_UAT_STAGE,
	COMPILE_STAGE,
	PACKAGE_STAGE,
	DEPLOY_STAGE,
};

class FOculusBuildAnalytics
{
public:
	static FOculusBuildAnalytics* GetInstance();

	void OnLauncherCreated(ILauncherRef Launcher);
	void OnLauncherWorkerStarted(ILauncherWorkerPtr LauncherWorker, ILauncherProfileRef Profile);
	void OnStageCompleted(const FString& StageName, double Time);
	void OnStageStarted(const FString& StageName);
	void OnBuildOutputRecieved(const FString& Message);

private:
	FOculusBuildAnalytics();

	static FOculusBuildAnalytics* instance;

	float AndroidPackageTime;
	bool UATLaunched;
	int UserAssetCount;
	int32 SourceFileCount;
	int64 SourceFileDirectorySize;

	EBuildStage CurrentBuildStage;
	FString CurrentBuildPlatform;
};