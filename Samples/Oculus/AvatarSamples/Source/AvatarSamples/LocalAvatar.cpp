// Fill out your copyright notice in the Description page of Project Settings.

#include "AvatarSamples.h"
#include "LocalAvatar.h"
#include "OvrAvatarManager.h"
#include "RemoteAvatar.h"
#include "Object.h"
#include "OVRLipSyncLiveActorComponent.h"
#include "OVRLipSyncPlaybackActorComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWave.h"
#include "IConsoleManager.h"

static const uint32_t HAND_JOINTS = 25;
DEFINE_LOG_CATEGORY(LogAvatarSamples);


std::unordered_map<AvatarLevelOfDetail, ovrAvatarAssetLevelOfDetail, AvatarLevelOfDetailHash> ALocalAvatar::LODMap
(
	{ 
		{ AvatarLevelOfDetail::Low, ovrAvatarAssetLevelOfDetail_One},
		{ AvatarLevelOfDetail::Mid, ovrAvatarAssetLevelOfDetail_Three},
		{ AvatarLevelOfDetail::High, ovrAvatarAssetLevelOfDetail_Five}
	}
);

ovrAvatarTransform gAvatarLeftHandTrans[] =
{
	{ { 0.0000f,0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0400f,-0.0250f,0.1091f },{ -0.6825f, 0.1749f, 0.7085f, 0.0411f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0735f,-0.0140f,-0.0240f },{ -0.3899f, 0.7092f, -0.1519f, 0.5674f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.1361f,0.0000f,-0.0000f },{ -0.2270f, 0.0000f, -0.0000f, 0.9739f },{ 1.0000f,1.0000f,1.0000f } },

	{ { 0.0871f,-0.0351f,0.0068f },{ -0.3804f, 0.6000f, -0.5778f, -0.4017f },{ 1.0000f,1.0000f,1.0000f } }, //Index
	{ { -0.0416f,-0.0000f,-0.0000f },{ -0.0000f, 0.0000f, -0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0222f,0.0000f,0.0000f },{ -0.0000f, 0.0000f, -0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0291f,0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { 0.0914f,-0.0095f,0.0108f },{ 0.4631f, -0.4423f, 0.5945f, 0.4863f },{ 1.0000f,1.0000f,1.0000f } }, //Middle
	{ { -0.0460f,-0.0000f,-0.0000f },{ 0.0000f, -0.0000f, -0.8362f, 0.5484f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0296f,0.0000f,0.0000f },{ -0.0000f, -0.0000f, -0.7300f, 0.6834f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0265f,0.0000f,-0.0000f },{ -0.0000f, 0.0000f, -0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { 0.0313f,0.0191f,-0.0115f },{ 0.4713f, 0.0618f, 0.0753f, 0.8766f },{ 1.0000f,1.0000f,1.0000f } }, //Pinky
	{ { 0.0536f,0.0024f,0.0015f },{ 0.1300f, 0.0348f, 0.6327f, 0.7626f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0334f,0.0000f,-0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0174f,-0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0194f,0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { 0.0895f,0.0127f,0.0019f },{ 0.4589f, -0.3678f, 0.6193f, 0.5203f },{ 1.0000f,1.0000f,1.0000f } }, //Ring
	{ { -0.0386f,0.0000f,-0.0000f },{ -0.0000f, -0.0000f, -0.8446f, 0.5354f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0258f,-0.0000f,0.0000f },{ 0.0000f, -0.0000f, -0.7372f, 0.6757f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0242f,-0.0000f,0.0000f },{ -0.0000f, -0.0000f, -0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { 0.0309f,-0.0415f,-0.0206f },{ 0.1999f, 0.9526f, 0.0626f, -0.2205f },{ 1.0000f,1.0000f,1.0000f } }, //Thumb
	{ { -0.0326f,0.0000f,-0.0000f },{ -0.0087f, 0.0964f, -0.2674f, 0.9587f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0264f,0.0000f,-0.0000f },{ -0.0000f, 0.0000f, -0.5985f, 0.8011f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0341f,0.0000f,0.0000f },{ -0.0000f, -0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
};

ovrAvatarTransform gAvatarRightHandTrans[] =
{
	{ { 0.0000f,0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0400f,-0.0250f,0.1091f },{ 0.0411f, -0.7085f, 0.1749f, 0.6825f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0735f,0.0140f,0.0240f },{ -0.5702f, -0.0164f, 0.8065f, -0.1554f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.1361f,0.0000f,-0.0000f },{ -0.2270f, -0.0000f, 0.0000f, 0.9739f },{ 1.0000f,1.0000f,1.0000f } },

	{ { -0.0871f,0.0351f,-0.0068f },{ -0.3804f, 0.6000f, -0.5778f, -0.4017f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0416f,-0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0222f,-0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0291f,0.0000f,-0.0000f },{ -0.0000f, -0.0000f, -0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { -0.0914f,0.0095f,-0.0108f },{ 0.4631f, -0.4423f, 0.5945f, 0.4863f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0460f,0.0000f,0.0000f },{ 0.0000f, -0.0000f, -0.8362f, 0.5484f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0296f,-0.0000f,0.0000f },{ 0.0000f, -0.0000f, -0.7300f, 0.6834f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0265f,0.0000f,-0.0000f },{ 0.0000f, -0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { -0.0313f,-0.0191f,0.0115f },{ 0.4713f, 0.0618f, 0.0753f, 0.8766f },{ 1.0000f,1.0000f,1.0000f } },
	{ { -0.0536f,-0.0024f,-0.0015f },{ 0.1300f, 0.0348f, 0.6327f, 0.7626f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0334f,0.0000f,-0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0174f,-0.0000f,0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0194f,0.0000f,-0.0000f },{ 0.0000f, 0.0000f, 0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { -0.0895f,-0.0127f,-0.0019f },{ 0.4589f, -0.3678f, 0.6193f, 0.5203f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0386f,0.0000f,0.0000f },{ -0.0000f, -0.0000f, -0.8446f, 0.5354f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0258f,0.0000f,-0.0000f },{ -0.0000f, 0.0000f, -0.7372f, 0.6757f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0242f,-0.0000f,-0.0000f },{ 0.0000f, 0.0000f, -0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },

	{ { -0.0309f,0.0415f,0.0206f },{ 0.1999f, 0.9526f, 0.0626f, -0.2205f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0326f,0.0000f,0.0000f },{ -0.0087f, 0.0964f, -0.2674f, 0.9587f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0264f,-0.0000f,-0.0000f },{ 0.0000f, -0.0000f, -0.5985f, 0.8011f },{ 1.0000f,1.0000f,1.0000f } },
	{ { 0.0341f,0.0000f,-0.0000f },{ 0.0000f, -0.0000f, -0.0000f, 1.0000f },{ 1.0000f,1.0000f,1.0000f } },
};

ALocalAvatar::ALocalAvatar()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("LocalAvatarRoot"));

	PrimaryActorTick.bCanEverTick = true;

	AvatarComponent = CreateDefaultSubobject<UOvrAvatar>(TEXT("LocalAvatar"));
	PlayBackLipSyncComponent = CreateDefaultSubobject<UOVRLipSyncPlaybackActorComponent>(TEXT("CannedLipSync"));
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("LocalAvatarAudio"));
	LipSyncComponent = CreateDefaultSubobject<UOVRLipSyncActorComponent>(TEXT("LocalLipSync"));
}

void ALocalAvatar::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	if (UseCannedLipSyncPlayback)
	{
		FString playbackAssetPath = TEXT("/Game/Audio/vox_lp_01_LipSyncSequence");
		auto sequence = LoadObject<UOVRLipSyncFrameSequence>(nullptr, *playbackAssetPath, nullptr, LOAD_None, nullptr);
		PlayBackLipSyncComponent->Sequence = sequence;

		FString AudioClip = TEXT("/Game/Audio/vox_lp_01");
		auto SoundWave = LoadObject<USoundWave>(nullptr, *AudioClip, nullptr, LOAD_None, nullptr);

		if (SoundWave)
		{
			SoundWave->bLooping = 1;
			AudioComponent->Sound = SoundWave;
		}
	}
#if PLATFORM_WINDOWS
	else
	{
		auto SilenceDetectionThresholdCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("voice.SilenceDetectionThreshold"));
		SilenceDetectionThresholdCVar->Set(0.f);
	}
#endif

	AvatarComponent->SetVisibilityType(
		AvatarVisibilityType == AvatarVisibility::FirstPerson 
		? ovrAvatarVisibilityFlag_FirstPerson 
		: ovrAvatarVisibilityFlag_ThirdPerson);

	AvatarComponent->SetExpressiveCapability(EnableExpressive);
	AvatarComponent->SetBodyCapability(EnableBody);
	AvatarComponent->SetHandsCapability(EnableHands);
	AvatarComponent->SetBaseCapability(EnableBase);

	AvatarComponent->SetBodyMaterial(GetOvrAvatarMaterialFromType(BodyMaterial));
	AvatarComponent->SetHandMaterial(GetOvrAvatarMaterialFromType(HandsMaterial));
}

void ALocalAvatar::BeginPlay()
{
	Super::BeginPlay();

	uint64 UserID = FCString::Strtoui64(*OculusUserId, NULL, 10);

	IOnlineIdentityPtr IdentityInterface = Online::GetIdentityInterface();
	if (IdentityInterface.IsValid())
	{
		OnLoginCompleteDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &ALocalAvatar::OnLoginComplete));
		IdentityInterface->AutoLogin(0);
	}

	if (UseCannedLipSyncPlayback)
	{
		PlayBackLipSyncComponent->OnVisemesReady.AddDynamic(this, &ALocalAvatar::LipSyncVismesReady);
	}
	else if (UseLocalMicrophone)
	{
		LipSyncComponent->OnVisemesReady.AddDynamic(this, &ALocalAvatar::LipSyncVismesReady);
		LipSyncComponent->Start();
	}
}

void ALocalAvatar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UseCannedLipSyncPlayback)
	{
		PlayBackLipSyncComponent->OnVisemesReady.RemoveDynamic(this, &ALocalAvatar::LipSyncVismesReady);
	}
	else if (UseLocalMicrophone)
	{
		LipSyncComponent->OnVisemesReady.RemoveDynamic(this, &ALocalAvatar::LipSyncVismesReady);
	}
}

void ALocalAvatar::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdatePacketRecording(DeltaTime);
}

void ALocalAvatar::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	IOnlineIdentityPtr OculusIdentityInterface = Online::GetIdentityInterface();
	OculusIdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, OnLoginCompleteDelegateHandle);

	if (AvatarComponent)
	{
		uint64 ID = FCString::Strtoui64(*OculusUserId, NULL, 10);
		AvatarComponent->RequestAvatar(ID, LODMap[LevelOfDetail], UseCombinedMesh);
	}
}

void ALocalAvatar::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);

	Input->BindAction("AvatarCycleRightHandPose", IE_Pressed, this, &ALocalAvatar::CycleRightHandPose);
	Input->BindAction("AvatarCycleLeftHandPose", IE_Pressed, this, &ALocalAvatar::CycleLeftHandPose);
}

void ALocalAvatar::CycleRightHandPose()
{
	switch (RightHandPoseIndex)
	{
	case eHandPoseState::Default:
		RightHandPoseIndex = eHandPoseState::Sphere;
		AvatarComponent->SetRightHandPose(ovrAvatarHandGesture_GripSphere);
		break;
	case eHandPoseState::Sphere:
		RightHandPoseIndex = eHandPoseState::Cube;
		AvatarComponent->SetRightHandPose(ovrAvatarHandGesture_GripCube);
		break;
	case eHandPoseState::Cube:
		RightHandPoseIndex = eHandPoseState::Custom;
		AvatarComponent->SetCustomGesture(UOvrAvatar::HandType_Right, gAvatarRightHandTrans, HAND_JOINTS);
		break;
	case eHandPoseState::Custom:
		AvatarComponent->SetRightHandPose(ovrAvatarHandGesture_Default);
		AvatarComponent->SetControllerVisibility(UOvrAvatar::HandType_Right, true);
		RightHandPoseIndex = eHandPoseState::Controller;
		break;
	case eHandPoseState::Controller:
		RightHandPoseIndex = eHandPoseState::Default;
		AvatarComponent->SetControllerVisibility(UOvrAvatar::HandType_Right, false);
		break;
	case eHandPoseState::Detached:
		break;
	default:
		break;
	}
}

void ALocalAvatar::CycleLeftHandPose()
{
	switch (LeftHandPoseIndex)
	{
	case eHandPoseState::Default:
		LeftHandPoseIndex = eHandPoseState::Sphere;
		AvatarComponent->SetLeftHandPose(ovrAvatarHandGesture_GripSphere);
		break;
	case eHandPoseState::Sphere:
		LeftHandPoseIndex = eHandPoseState::Cube;
		AvatarComponent->SetLeftHandPose(ovrAvatarHandGesture_GripCube);
		break;
	case eHandPoseState::Cube:
		LeftHandPoseIndex = eHandPoseState::Custom;
		AvatarComponent->SetCustomGesture(UOvrAvatar::HandType_Left, gAvatarLeftHandTrans, HAND_JOINTS);
		break;
	case eHandPoseState::Custom:
		AvatarComponent->SetLeftHandPose(ovrAvatarHandGesture_Default);
		AvatarComponent->SetControllerVisibility(UOvrAvatar::HandType_Left, true);
		LeftHandPoseIndex = eHandPoseState::Controller;
		break;
	case eHandPoseState::Controller:
		LeftHandPoseIndex = eHandPoseState::Default;
		AvatarComponent->SetControllerVisibility(UOvrAvatar::HandType_Left, false);
		break;
	case eHandPoseState::Detached:
		break;
	default:
		break;
	}
}

void ALocalAvatar::UpdatePacketRecording(float DeltaTime)
{
	if (!AvatarComponent)
		return;

	if (!PacketSettings.Initialized)
	{
		AvatarComponent->StartPacketRecording();
		PacketSettings.AccumulatedTime = 0.f;
		PacketSettings.RecordingFrames = true;
		PacketSettings.Initialized = true;
	}

	if (PacketSettings.RecordingFrames)
	{
		PacketSettings.AccumulatedTime += DeltaTime;

		if (PacketSettings.AccumulatedTime >= PacketSettings.UpdateRate)
		{
			PacketSettings.AccumulatedTime = 0.f;
			UOvrAvatarManager::Get().QueueAvatarPacket(AvatarComponent->EndPacketRecording());
			AvatarComponent->StartPacketRecording();
		}
	}
}

void ALocalAvatar::LipSyncVismesReady()
{
	if (UseCannedLipSyncPlayback)
	{
		AvatarComponent->UpdateVisemeValues(PlayBackLipSyncComponent->GetVisemes());
	}
	else
	{
		AvatarComponent->UpdateVisemeValues(LipSyncComponent->GetVisemes());
	}
}

UOvrAvatar::MaterialType ALocalAvatar::GetOvrAvatarMaterialFromType(AvatarMaterial material)
{
	switch (material)
	{
	case AvatarMaterial::Masked:
		return UOvrAvatar::MaterialType::Masked;
	case AvatarMaterial::Translucent:
		return UOvrAvatar::MaterialType::Translucent;
	case AvatarMaterial::Opaque:
		return UOvrAvatar::MaterialType::Opaque;
	}

	return UOvrAvatar::MaterialType::Opaque;
}
