// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusMR_CastingCameraActor.h"

#include "OculusMRPrivate.h"
#include "OculusHMD_Settings.h"
#include "OculusHMD.h"
#include "OculusMR_CastingWindowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "OculusMR_PlaneMeshComponent.h"
#include "OculusMR_BoundaryActor.h"
#include "OculusMR_BoundaryMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Texture2D.h"
#include "RenderingThread.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"
#include "VRNotificationsComponent.h"
#include "RenderUtils.h"

#define LOCTEXT_NAMESPACE "OculusMR_CastingCameraActor"

static TAutoConsoleVariable<int32> CAutoOpenCastingVar(TEXT("mr.AutoOpenCasting"), 0, TEXT("Auto open casting: 1=MultiView; 2=DirectComposition"));
static TAutoConsoleVariable<int32> CProjectToMirrorWindowVar(TEXT("mr.ProjectToMirrorWindow"), 0, TEXT("Casting To MirrorWindow"));

static TAutoConsoleVariable<int32> COverrideMixedRealityParametersVar(TEXT("mr.MixedReality_Override"), 0, TEXT("Use the Mixed Reality console variables"));
static TAutoConsoleVariable<int32> CChromaKeyColorRVar(TEXT("mr.MixedReality_ChromaKeyColor_R"), 0, TEXT("Chroma Key Color R"));
static TAutoConsoleVariable<int32> CChromaKeyColorGVar(TEXT("mr.MixedReality_ChromaKeyColor_G"), 255, TEXT("Chroma Key Color G"));
static TAutoConsoleVariable<int32> CChromaKeyColorBVar(TEXT("mr.MixedReality_ChromaKeyColor_B"), 0, TEXT("Chroma Key Color B"));
static TAutoConsoleVariable<float> CChromaKeySimilarityVar(TEXT("mr.MixedReality_ChromaKeySimilarity"), 0.6f, TEXT("Chroma Key Similarity"));
static TAutoConsoleVariable<float> CChromaKeySmoothRangeVar(TEXT("mr.MixedReality_ChromaKeySmoothRange"), 0.03f, TEXT("Chroma Key Smooth Range"));
static TAutoConsoleVariable<float> CChromaKeySpillRangeVar(TEXT("mr.MixedReality_ChromaKeySpillRange"), 0.04f, TEXT("Chroma Key Spill Range"));
static TAutoConsoleVariable<float> CCastingLantencyVar(TEXT("mr.MixedReality_CastingLantency"), 0, TEXT("Casting Latency"));

namespace
{
	ovrpCameraDevice ConvertCameraDevice(EOculusMR_CameraDeviceEnum device)
	{
		if (device == EOculusMR_CameraDeviceEnum::CD_WebCamera0)
			return ovrpCameraDevice_WebCamera0;
		else if (device == EOculusMR_CameraDeviceEnum::CD_WebCamera1)
			return ovrpCameraDevice_WebCamera1;
		else if (device == EOculusMR_CameraDeviceEnum::CD_ZEDCamera)
			return ovrpCameraDevice_ZEDStereoCamera;
		checkNoEntry();
		return ovrpCameraDevice_None;
	}

	ovrpCameraDeviceDepthQuality ConvertCameraDepthQuality(EOculusMR_DepthQuality depthQuality)
	{
		if (depthQuality == EOculusMR_DepthQuality::DQ_Low)
		{
			return ovrpCameraDeviceDepthQuality_Low;
		}
		else if (depthQuality == EOculusMR_DepthQuality::DQ_Medium)
		{
			return ovrpCameraDeviceDepthQuality_Medium;
		}
		else if (depthQuality == EOculusMR_DepthQuality::DQ_High)
		{
			return ovrpCameraDeviceDepthQuality_High;
		}
		checkNoEntry();
		return ovrpCameraDeviceDepthQuality_Medium;
	}

	bool GetCameraTrackedObjectPoseInTrackingSpace(OculusHMD::FOculusHMD* OculusHMD, const FTrackedCamera& TrackedCamera, OculusHMD::FPose& CameraTrackedObjectPose)
	{
		using namespace OculusHMD;

		CameraTrackedObjectPose = FPose(FQuat::Identity, FVector::ZeroVector);

		if (TrackedCamera.AttachedTrackedDevice != ETrackedDeviceType::None)
		{
			ovrpResult result = ovrpSuccess;
			ovrpPoseStatef cameraPoseState;
			ovrpNode deviceNode = ToOvrpNode(TrackedCamera.AttachedTrackedDevice);
			ovrpBool nodePresent = ovrpBool_False;
			result = ovrp_GetNodePresent2(deviceNode, &nodePresent);
			if (!OVRP_SUCCESS(result))
			{
				UE_LOG(LogMR, Warning, TEXT("Unable to check if AttachedTrackedDevice is present"));
				return false;
			}
			if (!nodePresent)
			{
				UE_LOG(LogMR, Warning, TEXT("AttachedTrackedDevice is not present"));
				return false;
			}

			OculusHMD::FGameFrame* CurrentFrame;
			if (IsInGameThread())
			{
				CurrentFrame = OculusHMD->GetNextFrameToRender();
			}
			else
			{
				CurrentFrame = OculusHMD->GetFrame_RenderThread();
			}

			result = CurrentFrame ? ovrp_GetNodePoseState3(ovrpStep_Render, CurrentFrame->FrameNumber, deviceNode, &cameraPoseState) : ovrpFailure;
			if (!OVRP_SUCCESS(result))
			{
				UE_LOG(LogMR, Warning, TEXT("Unable to retrieve AttachedTrackedDevice pose state"));
				return false;
			}
			OculusHMD->ConvertPose(cameraPoseState.Pose, CameraTrackedObjectPose);
		}

		return true;
	}
}

//////////////////////////////////////////////////////////////////////////
// ACastingCameraActor

AOculusMR_CastingCameraActor::AOculusMR_CastingCameraActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TrackedCamera()
	, TrackingReferenceComponent(NULL)
	, bFollowTrackingReference(true)
	, bCastingAutoStart(false)
	, bProjectToMirrorWindow(false)
	, CompositionMethod(ECastingViewportCompositionMethod::MultiView)
	, ClippingReference(EOculusMR_ClippingReference::CR_Head)
	, bUseTrackedCameraResolution(true)
	, WidthPerView(960)
	, HeightPerView(540)
	, CapturingCamera(EOculusMR_CameraDeviceEnum::CD_WebCamera0)
	, CastingLatency(0.0f)
	, HandPoseStateLatency(0.0f)
	, ChromaKeyColor(FColor::Green)
	, ChromaKeySimilarity(0.6f)
	, ChromaKeySmoothRange(0.03f)
	, ChromaKeySpillRange(0.04f)
	, VirtualGreenScreenType(EOculusMR_VirtualGreenScreenType::VGS_Off)
	, bUseDynamicLighting(false)
	, DepthQuality(EOculusMR_DepthQuality::DQ_Medium)
	, DynamicLightingDepthSmoothFactor(8.0f)
	, DynamicLightingDepthVariationClampingValue(0.001f)
	, CurrentCapturingCamera(ovrpCameraDevice_None)
	, ChromaKeyMaterial(NULL)
	, ChromaKeyLitMaterial(NULL)
	, ChromaKeyMaterialInstance(NULL)
	, ChromaKeyLitMaterialInstance(NULL)
	, CameraFrameMaterialInstance(NULL)
	, TrackedCameraCalibrationRequired(false)
	, HasTrackedCameraCalibrationCalibrated(false)
	, RefreshBoundaryMeshCounter(3)
	, BindToTrackedCameraIndexRequested(false)
	, BindToTrackedCameraIndex(-1)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	CastingWindowComponent = CreateDefaultSubobject<UOculusMR_CastingWindowComponent>(TEXT("OutputWindowComponent"));

	VRNotificationComponent = CreateDefaultSubobject<UVRNotificationsComponent>(TEXT("VRNotificationComponent"));

	PlaneMeshComponent = CreateDefaultSubobject<UOculusMR_PlaneMeshComponent>(TEXT("PlaneMeshComponent"));
	PlaneMeshComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PlaneMeshComponent->ResetRelativeTransform();
	PlaneMeshComponent->SetVisibility(false);

	ChromaKeyMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/OculusVR/Materials/OculusMR_ChromaKey")));
	if (!ChromaKeyMaterial)
	{
		UE_LOG(LogMR, Warning, TEXT("Invalid ChromaKeyMaterial"));
	}

	ChromaKeyLitMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/OculusVR/Materials/OculusMR_ChromaKey_Lit")));
	if (!ChromaKeyLitMaterial)
	{
		UE_LOG(LogMR, Warning, TEXT("Invalid ChromaKeyLitMaterial"));
	}

	OpaqueColoredMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/OculusVR/Materials/OculusMR_OpaqueColoredMaterial")));
	if (!OpaqueColoredMaterial)
	{
		UE_LOG(LogMR, Warning, TEXT("Invalid OpaqueColoredMaterial"));
	}

	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UTexture2D> WhiteSquareTexture;
		FConstructorStatics()
			: WhiteSquareTexture(TEXT("/Engine/EngineResources/WhiteSquareTexture"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	DefaultTexture_White = ConstructorStatics.WhiteSquareTexture.Object;
	check(DefaultTexture_White);
}

void AOculusMR_CastingCameraActor::SaveToIni()
{
	if (!GConfig)
	{
		UE_LOG(LogMR, Warning, TEXT("GConfig is NULL"));
		return;
	}

	const TCHAR* OculusMRSettings = TEXT("Oculus.Settings.MixedReality");
	GConfig->SetBool(OculusMRSettings, TEXT("bCastingAutoStart"), bCastingAutoStart, GEngineIni);
	GConfig->SetBool(OculusMRSettings, TEXT("bProjectToMirrorWindow"), bProjectToMirrorWindow, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("CompositionMethod"), (int32)CompositionMethod, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("ClippingReference"), (int32)ClippingReference, GEngineIni);
	GConfig->SetBool(OculusMRSettings, TEXT("bFollowTrackingReference"), bFollowTrackingReference, GEngineIni);
	GConfig->SetBool(OculusMRSettings, TEXT("bUseTrackedCameraResolution"), bUseTrackedCameraResolution, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("WidthPerView"), WidthPerView, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("HeightPerView"), HeightPerView, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("CapturingCamera"), (int32)CapturingCamera, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("CastingLatency"), CastingLatency, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("HandPoseStateLatency"), HandPoseStateLatency, GEngineIni);
	GConfig->SetColor(OculusMRSettings, TEXT("ChromaKeyColor"), ChromaKeyColor, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("ChromaKeySimilarity"), ChromaKeySimilarity, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("ChromaKeySmoothRange"), ChromaKeySmoothRange, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("ChromaKeySpillRange"), ChromaKeySpillRange, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("VirtualGreenScreenType"), (int32)VirtualGreenScreenType, GEngineIni);
	GConfig->SetBool(OculusMRSettings, TEXT("bUseDynamicLighting"), bUseDynamicLighting, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("DepthQuality"), (int32)DepthQuality, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("DynamicLightingDepthSmoothFactor"), DynamicLightingDepthSmoothFactor, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("DynamicLightingDepthVariationClampingValue"), DynamicLightingDepthVariationClampingValue, GEngineIni);

	GConfig->Flush(false, GEngineIni);

	UE_LOG(LogMR, Log, TEXT("MixedReality settings saved to Engine.ini"));
}

void AOculusMR_CastingCameraActor::LoadFromIni()
{
	if (!GConfig)
	{
		UE_LOG(LogMR, Warning, TEXT("GConfig is NULL"));
		return;
	}

	// Flushing the GEngineIni is necessary to get the settings reloaded at the runtime, but the manual flushing
	// could cause an assert when loading audio settings if launching through editor at the 2nd time. Disabled temporarily.
	//GConfig->Flush(true, GEngineIni);

	const TCHAR* OculusMRSettings = TEXT("Oculus.Settings.MixedReality");
	bool v;
	float f;
	int32 i;
	FVector vec;
	FColor color;
	if (GConfig->GetBool(OculusMRSettings, TEXT("bCastingAutoStart"), v, GEngineIni))
	{
		bCastingAutoStart = v;
	}
	if (GConfig->GetBool(OculusMRSettings, TEXT("bProjectToMirrorWindow"), v, GEngineIni))
	{
		bProjectToMirrorWindow = v;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("CompositionMethod"), i, GEngineIni))
	{
		CompositionMethod = (ECastingViewportCompositionMethod)i;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("ClippingReference"), i, GEngineIni))
	{
		ClippingReference = (EOculusMR_ClippingReference)i;
	}
	if (GConfig->GetBool(OculusMRSettings, TEXT("bFollowTrackingReference"), v, GEngineIni))
	{
		bFollowTrackingReference = v;
	}
	if (GConfig->GetBool(OculusMRSettings, TEXT("bUseTrackedCameraResolution"), v, GEngineIni))
	{
		bUseTrackedCameraResolution = v;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("WidthPerView"), i, GEngineIni))
	{
		WidthPerView = i;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("HeightPerView"), i, GEngineIni))
	{
		HeightPerView = i;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("CapturingCamera"), i, GEngineIni))
	{
		CapturingCamera = (EOculusMR_CameraDeviceEnum)i;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("CastingLatency"), f, GEngineIni))
	{
		CastingLatency = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("HandPoseStateLatency"), f, GEngineIni))
	{
		HandPoseStateLatency = f;
	}
	if (GConfig->GetColor(OculusMRSettings, TEXT("ChromaKeyColor"), color, GEngineIni))
	{
		ChromaKeyColor = color;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("ChromaKeySimilarity"), f, GEngineIni))
	{
		ChromaKeySimilarity = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("ChromaKeySmoothRange"), f, GEngineIni))
	{
		ChromaKeySmoothRange = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("ChromaKeySpillRange"), f, GEngineIni))
	{
		ChromaKeySpillRange = f;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("VirtualGreenScreenType"), i, GEngineIni))
	{
		VirtualGreenScreenType = (EOculusMR_VirtualGreenScreenType)i;
	}
	if (GConfig->GetBool(OculusMRSettings, TEXT("bUseDynamicLighting"), v, GEngineIni))
	{
		bUseDynamicLighting = v;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("DepthQuality"), i, GEngineIni))
	{
		DepthQuality = (EOculusMR_DepthQuality)i;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("DynamicLightingDepthSmoothFactor"), f, GEngineIni))
	{
		DynamicLightingDepthSmoothFactor = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("DynamicLightingDepthVariationClampingValue"), f, GEngineIni))
	{
		DynamicLightingDepthVariationClampingValue = f;
	}

	UE_LOG(LogMR, Log, TEXT("MixedReality settings loaded from Engine.ini"));
}

void AOculusMR_CastingCameraActor::BeginDestroy()
{
	CloseCastingWindow();
	Super::BeginDestroy();
}

bool AOculusMR_CastingCameraActor::RefreshExternalCamera()
{
	using namespace OculusHMD;

	if (TrackedCamera.Index >= 0)
	{
		int cameraCount;
		if (OVRP_FAILURE(ovrp_GetExternalCameraCount(&cameraCount)))
		{
			cameraCount = 0;
		}
		if (TrackedCamera.Index >= cameraCount)
		{
			UE_LOG(LogMR, Error, TEXT("Invalid TrackedCamera Index"));
			return false;
		}
		FOculusHMD* OculusHMD = GEngine->XRSystem.IsValid() ? (FOculusHMD*)(GEngine->XRSystem->GetHMDDevice()) : nullptr;
		if (!OculusHMD)
		{
			UE_LOG(LogMR, Error, TEXT("Unable to retrieve OculusHMD"));
			return false;
		}
		ovrpResult result = ovrpSuccess;
		ovrpCameraExtrinsics cameraExtrinsics;
		result = ovrp_GetExternalCameraExtrinsics(TrackedCamera.Index, &cameraExtrinsics);
		if (OVRP_FAILURE(result))
		{
			UE_LOG(LogMR, Error, TEXT("ovrp_GetExternalCameraExtrinsics failed"));
			return false;
		}
		TrackedCamera.AttachedTrackedDevice = OculusHMD::ToETrackedDeviceType(cameraExtrinsics.AttachedToNode);
		OculusHMD::FPose Pose;
		OculusHMD->ConvertPose(cameraExtrinsics.RelativePose, Pose);
		TrackedCamera.CalibratedRotation = Pose.Orientation.Rotator();
		TrackedCamera.CalibratedOffset = Pose.Position;
	}

	return true;
}

void AOculusMR_CastingCameraActor::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() && (GetWorld()->WorldType == EWorldType::Game || GetWorld()->WorldType == EWorldType::None))
	{
		// MxR casting would not automatically started in a standalone game, unless one of the -mxr_open parameters is provided
		bCastingAutoStart = false;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("mxr_project_to_mirror_window")) || (CProjectToMirrorWindowVar.GetValueOnAnyThread() > 0))
	{
		bProjectToMirrorWindow = true;
	}

	const bool bAutoOpen = FParse::Param(FCommandLine::Get(), TEXT("mxr_open"));
	if (bAutoOpen)
	{
		bCastingAutoStart = true;
	}

	const bool bAutoOpenInMultiView = FParse::Param(FCommandLine::Get(), TEXT("mxr_open_multiview")) || (CAutoOpenCastingVar.GetValueOnAnyThread() == 1);
	const bool bAutoOpenInDirectComposition = FParse::Param(FCommandLine::Get(), TEXT("mxr_open_direct_composition")) || (CAutoOpenCastingVar.GetValueOnAnyThread() == 2);

	if (FParse::Param(FCommandLine::Get(), TEXT("load_mxr_settings")))
	{
		LoadFromIni();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("save_mxr_settings")))
	{
		SaveToIni();
	}

	if (bAutoOpenInMultiView)
	{
		CompositionMethod = ECastingViewportCompositionMethod::MultiView;
		OpenCastingWindow();
	}
	else if (bAutoOpenInDirectComposition)
	{
		CompositionMethod = ECastingViewportCompositionMethod::DirectComposition;
		OpenCastingWindow();
	}
	else if (bCastingAutoStart)
	{
		OpenCastingWindow();
	}

	GetCameraComponent()->bLockToHmd = false;

	BoundaryActor = GetWorld()->SpawnActor<AOculusMR_BoundaryActor>(AOculusMR_BoundaryActor::StaticClass());
	BoundaryActor->SetActorTransform(FTransform::Identity);

	BoundarySceneCaptureActor = GetWorld()->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass());
	BoundarySceneCaptureActor->GetCaptureComponent2D()->CaptureSource = SCS_SceneColorHDRNoAlpha;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->CaptureStereoPass = eSSP_FULL;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->bCaptureEveryFrame = false;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->bCaptureOnMovement = false;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->ShowOnlyActorComponents(BoundaryActor);
	BoundarySceneCaptureActor->GetCaptureComponent2D()->ShowFlags.Fog = false;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->ShowFlags.PostProcessing = false;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->ShowFlags.Lighting = false;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->ShowFlags.DisableAdvancedFeatures();
	BoundarySceneCaptureActor->GetCaptureComponent2D()->bEnableClipPlane = false;
	BoundarySceneCaptureActor->GetCaptureComponent2D()->MaxViewDistanceOverride = 10000.0f;

	if (BoundarySceneCaptureActor->GetCaptureComponent2D()->TextureTarget)
	{
		BoundarySceneCaptureActor->GetCaptureComponent2D()->TextureTarget->ClearColor = FLinearColor::Black;
	}
	BoundaryActor->BoundaryMeshComponent->CastingCameraActor = this;

	RefreshBoundaryMesh();

	FScriptDelegate Delegate;
	Delegate.BindUFunction(this, FName(TEXT("OnHMDRecentered")));
	VRNotificationComponent->HMDRecenteredDelegate.Add(Delegate);
}

void AOculusMR_CastingCameraActor::EndPlay(EEndPlayReason::Type Reason)
{
	VRNotificationComponent->HMDRecenteredDelegate.Remove(this, FName(TEXT("OnHMDRecentered")));

	BoundaryActor->BoundaryMeshComponent->CastingCameraActor = NULL;

	BoundarySceneCaptureActor->Destroy();
	BoundarySceneCaptureActor = NULL;

	BoundaryActor->Destroy();
	BoundaryActor = NULL;

	CloseCastingWindow();
	Super::EndPlay(Reason);
}

void AOculusMR_CastingCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (BindToTrackedCameraIndexRequested)
	{
		Execute_BindToTrackedCameraIndexIfAvailable();
	}

	if (HasCastingWindowOpened())
	{
		if (!RefreshExternalCamera())
		{
			CloseCastingWindow();
			return;
		}

		if (COverrideMixedRealityParametersVar.GetValueOnAnyThread() > 0)
		{
			ChromaKeyColor = FColor(CChromaKeyColorRVar.GetValueOnAnyThread(), CChromaKeyColorGVar.GetValueOnAnyThread(), CChromaKeyColorBVar.GetValueOnAnyThread());
			ChromaKeySimilarity = CChromaKeySimilarityVar.GetValueOnAnyThread();
			ChromaKeySmoothRange = CChromaKeySmoothRangeVar.GetValueOnAnyThread();
			ChromaKeySpillRange = CChromaKeySpillRangeVar.GetValueOnAnyThread();
			CastingLatency = CCastingLantencyVar.GetValueOnAnyThread();
		}

		CastingWindowComponent->SetExpectedLantencyInSeconds(CastingLatency);

		if (CompositionMethod == ECastingViewportCompositionMethod::DirectComposition)
		{
			SetupCameraFrameMaterialInstance();

			if (CameraFrameMaterialInstance)
			{
				CameraFrameMaterialInstance->SetVectorParameterValue(FName(TEXT("ChromaKeyColor")), FLinearColor(ChromaKeyColor));
				CameraFrameMaterialInstance->SetScalarParameterValue(FName(TEXT("ChromaKeySimilarity")), ChromaKeySimilarity);
				CameraFrameMaterialInstance->SetScalarParameterValue(FName(TEXT("ChromaKeySmoothRange")), ChromaKeySmoothRange);
				CameraFrameMaterialInstance->SetScalarParameterValue(FName(TEXT("ChromaKeySpillRange")), ChromaKeySpillRange);
				if (bUseDynamicLighting)
				{
					CameraFrameMaterialInstance->SetScalarParameterValue(FName(TEXT("DepthSmoothFactor")), DynamicLightingDepthSmoothFactor);
					CameraFrameMaterialInstance->SetScalarParameterValue(FName(TEXT("DepthVariationClampingValue")), DynamicLightingDepthVariationClampingValue);
				}
			}
		}

		if (CurrentCapturingCamera != ovrpCameraDevice_None)
		{
			ovrpBool colorFrameAvailable = ovrpBool_False;
			ovrpSizei colorFrameSize = { 0, 0 };
			const ovrpByte* colorFrameData = nullptr;
			int colorRowPitch = 0;

			if (OVRP_SUCCESS(ovrp_IsCameraDeviceColorFrameAvailable2(CurrentCapturingCamera, &colorFrameAvailable)) && colorFrameAvailable &&
				OVRP_SUCCESS(ovrp_GetCameraDeviceColorFrameSize(CurrentCapturingCamera, &colorFrameSize)) &&
				OVRP_SUCCESS(ovrp_GetCameraDeviceColorFrameBgraPixels(CurrentCapturingCamera, &colorFrameData, &colorRowPitch)))
			{
				UpdateCameraColorTexture(colorFrameSize, colorFrameData, colorRowPitch);
			}

			ovrpBool supportDepth = ovrpBool_False;
			ovrpBool depthFrameAvailable = ovrpBool_False;
			ovrpSizei depthFrameSize = { 0, 0 };
			const float* depthFrameData = nullptr;
			int depthRowPitch = 0;
			if (bUseDynamicLighting &&
				OVRP_SUCCESS(ovrp_DoesCameraDeviceSupportDepth(CurrentCapturingCamera, &supportDepth)) && supportDepth &&
				OVRP_SUCCESS(ovrp_IsCameraDeviceDepthFrameAvailable(CurrentCapturingCamera, &depthFrameAvailable)) && depthFrameAvailable &&
				OVRP_SUCCESS(ovrp_GetCameraDeviceDepthFrameSize(CurrentCapturingCamera, &depthFrameSize)) &&
				OVRP_SUCCESS(ovrp_GetCameraDeviceDepthFramePixels(CurrentCapturingCamera, &depthFrameData, &depthRowPitch))
				)
			{
				UpdateCameraDepthTexture(depthFrameSize, depthFrameData, depthRowPitch);
			}
		}

		if (TrackedCameraCalibrationRequired)
		{
			CalibrateTrackedCameraPose();
		}
		UpdateTrackedCameraPosition();

		if (CompositionMethod == ECastingViewportCompositionMethod::DirectComposition)
		{
			UpdateBoundaryCapture();
		}

		RepositionPlaneMesh();

		double HandPoseStateLatencyToSet = (double)HandPoseStateLatency;
		ovrpResult result = ovrp_SetHandNodePoseStateLatency(HandPoseStateLatencyToSet);
		if (OVRP_FAILURE(result))
		{
			UE_LOG(LogMR, Warning, TEXT("ovrp_SetHandNodePoseStateLatency(%f) failed, result %d"), HandPoseStateLatencyToSet, (int)result);
		}
	}
}

void AOculusMR_CastingCameraActor::UpdateBoundaryCapture()
{
	if (VirtualGreenScreenType != EOculusMR_VirtualGreenScreenType::VGS_Off)
	{
		if (RefreshBoundaryMeshCounter > 0)
		{
			--RefreshBoundaryMeshCounter;
			BoundaryActor->BoundaryMeshComponent->MarkRenderStateDirty();
		}
		FVector TRLocation;
		FRotator TRRotation;
		if (UOculusMRFunctionLibrary::GetTrackingReferenceLocationAndRotationInWorldSpace(TrackingReferenceComponent, TRLocation, TRRotation))
		{
			FTransform TargetTransform(TRRotation, TRLocation);
			BoundaryActor->BoundaryMeshComponent->SetComponentToWorld(TargetTransform);
		}
		else
		{
			UE_LOG(LogMR, Warning, TEXT("Could not get the tracking reference transform"));
		}
	}

	if (VirtualGreenScreenType != EOculusMR_VirtualGreenScreenType::VGS_Off && BoundaryActor->IsBoundaryValid())
	{
		BoundaryActor->SetActorTransform(FTransform::Identity);
		if (VirtualGreenScreenType == EOculusMR_VirtualGreenScreenType::VGS_OuterBoundary)
		{
			if (BoundaryActor->BoundaryMeshComponent->BoundaryType != EOculusMR_BoundaryType::BT_OuterBoundary)
			{
				BoundaryActor->BoundaryMeshComponent->BoundaryType = EOculusMR_BoundaryType::BT_OuterBoundary;
				RefreshBoundaryMesh();
			}
		}
		else if (VirtualGreenScreenType == EOculusMR_VirtualGreenScreenType::VGS_PlayArea)
		{
			if (BoundaryActor->BoundaryMeshComponent->BoundaryType != EOculusMR_BoundaryType::BT_PlayArea)
			{
				BoundaryActor->BoundaryMeshComponent->BoundaryType = EOculusMR_BoundaryType::BT_PlayArea;
				RefreshBoundaryMesh();
			}
		}

		BoundarySceneCaptureActor->SetActorTransform(GetActorTransform());
		BoundarySceneCaptureActor->GetCaptureComponent2D()->FOVAngle = GetCameraComponent()->FieldOfView;
		UTextureRenderTarget2D* RenderTarget = BoundarySceneCaptureActor->GetCaptureComponent2D()->TextureTarget;

		int ViewWidth = bUseTrackedCameraResolution ? TrackedCamera.SizeX : WidthPerView;
		int ViewHeight = bUseTrackedCameraResolution ? TrackedCamera.SizeY : HeightPerView;
		if (RenderTarget == NULL || RenderTarget->GetSurfaceWidth() != ViewWidth || RenderTarget->GetSurfaceHeight() != ViewHeight)
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>();
			RenderTarget->ClearColor = FLinearColor::Black;
			RenderTarget->bAutoGenerateMips = false;
			RenderTarget->bGPUSharedFlag = false;
			RenderTarget->InitCustomFormat(ViewWidth, ViewHeight, PF_B8G8R8A8, false);
			BoundarySceneCaptureActor->GetCaptureComponent2D()->TextureTarget = RenderTarget;
		}
		BoundarySceneCaptureActor->GetCaptureComponent2D()->CaptureSceneDeferred();

		if (CameraFrameMaterialInstance)
		{
			CameraFrameMaterialInstance->SetTextureParameterValue(FName(TEXT("MaskTexture")), RenderTarget);
		}
	}
	else
	{
		if (CameraFrameMaterialInstance)
		{
			CameraFrameMaterialInstance->SetTextureParameterValue(FName(TEXT("MaskTexture")), DefaultTexture_White);
		}
	}
}

void AOculusMR_CastingCameraActor::UpdateCameraColorTexture(const ovrpSizei &frameSize, const ovrpByte* frameData, int rowPitch)
{
	if (CameraColorTexture->GetSizeX() != frameSize.w || CameraColorTexture->GetSizeY() != frameSize.h)
	{
		UE_LOG(LogMR, Log, TEXT("CameraColorTexture resize to (%d, %d)"), frameSize.w, frameSize.h);
		CameraColorTexture = UTexture2D::CreateTransient(frameSize.w, frameSize.h);
		CameraColorTexture->UpdateResource();
		if (CameraFrameMaterialInstance)
		{
			CameraFrameMaterialInstance->SetTextureParameterValue(FName(TEXT("CameraCaptureTexture")), CameraColorTexture);
			CameraFrameMaterialInstance->SetVectorParameterValue(FName(TEXT("CameraCaptureTextureSize")),
				FLinearColor((float)CameraColorTexture->GetSizeX(), (float)CameraColorTexture->GetSizeY(), 1.0f / CameraColorTexture->GetSizeX(), 1.0f / CameraColorTexture->GetSizeY()));
		}
		if (CastingWindowComponent)
		{
			CastingWindowComponent->SetCameraColorTexture(CameraColorTexture);
		}
	}
	uint32 Pitch = rowPitch;
	uint32 DataSize = frameSize.h * Pitch;
	uint8* SrcData = (uint8*)FMemory::Malloc(DataSize);
	FMemory::Memcpy(SrcData, frameData, DataSize);

	struct FUploadCameraTextureContext
	{
		uint8* CameraBuffer;	// Render thread assumes ownership
		uint32 CameraBufferPitch;
		FTexture2DResource* DestTextureResource;
		uint32 FrameWidth;
		uint32 FrameHeight;
	} UploadCameraTextureContext =
	{
		SrcData,
		Pitch,
		(FTexture2DResource*)CameraColorTexture->Resource,
		frameSize.w,
		frameSize.h
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		UpdateCameraColorTexture,
		FUploadCameraTextureContext, Context, UploadCameraTextureContext,
		{
			const FUpdateTextureRegion2D UpdateRegion(
				0, 0,		// Dest X, Y
				0, 0,		// Source X, Y
				Context.FrameWidth,	    // Width
				Context.FrameHeight	    // Height
			);

			RHIUpdateTexture2D(
				Context.DestTextureResource->GetTexture2DRHI(),	// Destination GPU texture
				0,												// Mip map index
				UpdateRegion,									// Update region
				Context.CameraBufferPitch,						// Source buffer pitch
				Context.CameraBuffer);							// Source buffer pointer

			FMemory::Free(Context.CameraBuffer);
		}
	);
}

void AOculusMR_CastingCameraActor::UpdateCameraDepthTexture(const ovrpSizei &frameSize, const float* frameData, int rowPitch)
{
	if (!CameraDepthTexture || CameraDepthTexture->GetSizeX() != frameSize.w || CameraDepthTexture->GetSizeY() != frameSize.h)
	{
		UE_LOG(LogMR, Log, TEXT("CameraDepthTexture resize to (%d, %d)"), frameSize.w, frameSize.h);
		CameraDepthTexture = UTexture2D::CreateTransient(frameSize.w, frameSize.h, PF_R32_FLOAT);
		CameraDepthTexture->UpdateResource();
		if (CameraFrameMaterialInstance && bUseDynamicLighting)
		{
			CameraFrameMaterialInstance->SetTextureParameterValue(FName(TEXT("CameraDepthTexture")), CameraDepthTexture);
		}
	}
	uint32 Pitch = rowPitch;
	uint32 DataSize = frameSize.h * Pitch;
	uint8* SrcData = (uint8*)FMemory::Malloc(DataSize);
	FMemory::Memcpy(SrcData, frameData, DataSize);

	struct FUploadCameraTextureContext
	{
		uint8* CameraBuffer;	// Render thread assumes ownership
		uint32 CameraBufferPitch;
		FTexture2DResource* DestTextureResource;
		uint32 FrameWidth;
		uint32 FrameHeight;
	} UploadCameraTextureContext =
	{
		SrcData,
		Pitch,
		(FTexture2DResource*)CameraDepthTexture->Resource,
		frameSize.w,
		frameSize.h
	};

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		UpdateCameraDepthTexture,
		FUploadCameraTextureContext, Context, UploadCameraTextureContext,
		{
			const FUpdateTextureRegion2D UpdateRegion(
				0, 0,		// Dest X, Y
				0, 0,		// Source X, Y
				Context.FrameWidth,	    // Width
				Context.FrameHeight	    // Height
			);

	RHIUpdateTexture2D(
		Context.DestTextureResource->GetTexture2DRHI(),	// Destination GPU texture
		0,												// Mip map index
		UpdateRegion,									// Update region
		Context.CameraBufferPitch,						// Source buffer pitch
		Context.CameraBuffer);							// Source buffer pointer

	FMemory::Free(Context.CameraBuffer);
		}
	);
}

void AOculusMR_CastingCameraActor::BindToTrackedCameraIndexIfAvailable(int InTrackedCameraIndex)
{
	BindToTrackedCameraIndexRequested = true;
	BindToTrackedCameraIndex = InTrackedCameraIndex;
}

void AOculusMR_CastingCameraActor::Execute_BindToTrackedCameraIndexIfAvailable()
{
	if (!BindToTrackedCameraIndexRequested)
	{
		return;
	}

	FTrackedCamera TempTrackedCamera;
	if (BindToTrackedCameraIndex >= 0)
	{
		TArray<FTrackedCamera> TrackedCameras;
		UOculusMRFunctionLibrary::GetAllTrackedCamera(TrackedCameras);
		int i;
		for (i = 0; i < TrackedCameras.Num(); ++i)
		{
			if (TrackedCameras[i].Index == BindToTrackedCameraIndex)
			{
				TempTrackedCamera = TrackedCameras[i];
				break;
			}
		}
		if (i == TrackedCameras.Num())
		{
			UE_LOG(LogMR, Warning, TEXT("Unable to find TrackedCamera at index %d, use TempTrackedCamera"), BindToTrackedCameraIndex);
		}
	}
	else
	{
		UE_LOG(LogMR, Warning, TEXT("BindToTrackedCameraIndex == %d, use TempTrackedCamera"), BindToTrackedCameraIndex);
	}

	TrackedCamera = TempTrackedCamera;
	if (TrackedCamera.Index < 0)
	{
		SetTrackedCameraUserPoseWithCameraTransform();
	}

	BindToTrackedCameraIndexRequested = false;
}

void AOculusMR_CastingCameraActor::RequestTrackedCameraCalibration()
{
	TrackedCameraCalibrationRequired = true;
}

void AOculusMR_CastingCameraActor::CalibrateTrackedCameraPose()
{
	SetTrackedCameraInitialPoseWithPlayerTransform();
	HasTrackedCameraCalibrationCalibrated = true;
	TrackedCameraCalibrationRequired = false;
}

void AOculusMR_CastingCameraActor::SetTrackedCameraInitialPoseWithPlayerTransform()
{
	using namespace OculusHMD;

	FOculusHMD* OculusHMD = GEngine->XRSystem.IsValid() ? (FOculusHMD*)(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	if (!OculusHMD)
	{
		UE_LOG(LogMR, Warning, TEXT("Unable to retrieve OculusHMD"));
		return;
	}

	FPose CameraTrackedObjectPose;
	if (!GetCameraTrackedObjectPoseInTrackingSpace(OculusHMD, TrackedCamera, CameraTrackedObjectPose))
	{
		return;
	}

	FPose CameraPose = CameraTrackedObjectPose * FPose(TrackedCamera.CalibratedRotation.Quaternion(), TrackedCamera.CalibratedOffset);
	CameraPose = CameraPose * FPose(TrackedCamera.UserRotation.Quaternion(), TrackedCamera.UserOffset);

	FQuat TROrientation;
	FVector TRLocation;
	FRotator TRRotation;
	if (!UOculusMRFunctionLibrary::GetTrackingReferenceLocationAndRotationInWorldSpace(TrackingReferenceComponent, TRLocation, TRRotation))
	{
		UE_LOG(LogMR, Warning, TEXT("Could not get player position"));
		return;
	}

	TROrientation = TRRotation.Quaternion();
	FPose FinalPose = FPose(TROrientation, TRLocation) * CameraPose;

	InitialCameraAbsoluteOrientation = FinalPose.Orientation;
	InitialCameraAbsolutePosition = FinalPose.Position;
	InitialCameraRelativeOrientation = CameraPose.Orientation;
	InitialCameraRelativePosition = CameraPose.Position;

	GetCameraComponent()->FieldOfView = TrackedCamera.FieldOfView;
}


void AOculusMR_CastingCameraActor::SetTrackedCameraUserPoseWithCameraTransform()
{
	using namespace OculusHMD;

	FOculusHMD* OculusHMD = GEngine->XRSystem.IsValid() ? (FOculusHMD*)(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	if (!OculusHMD)
	{
		UE_LOG(LogMR, Warning, TEXT("Unable to retrieve OculusHMD"));
		return;
	}

	FPose CameraTrackedObjectPose;
	if (!GetCameraTrackedObjectPoseInTrackingSpace(OculusHMD, TrackedCamera, CameraTrackedObjectPose))
	{
		return;
	}

	FPose CameraPose = CameraTrackedObjectPose * FPose(TrackedCamera.CalibratedRotation.Quaternion(), TrackedCamera.CalibratedOffset);


	FQuat TROrientation;
	FVector TRLocation;
	FRotator TRRotation;
	if (!UOculusMRFunctionLibrary::GetTrackingReferenceLocationAndRotationInWorldSpace(TrackingReferenceComponent, TRLocation, TRRotation))
	{
		UE_LOG(LogMR, Warning, TEXT("Could not get player position"));
		return;
	}
	TROrientation = TRRotation.Quaternion();
	FPose PlayerPose(TROrientation, TRLocation);
	FPose CurrentCameraPose = PlayerPose * CameraPose;

	FPose ExpectedCameraPose(GetCameraComponent()->GetComponentRotation().Quaternion(), GetCameraComponent()->GetComponentLocation());
	FPose UserPose = CurrentCameraPose.Inverse() * ExpectedCameraPose;

	TrackedCamera.UserRotation = UserPose.Orientation.Rotator();
	TrackedCamera.UserOffset = UserPose.Position;
}

void AOculusMR_CastingCameraActor::UpdateTrackedCameraPosition()
{
	check(HasTrackedCameraCalibrationCalibrated);

	using namespace OculusHMD;

	FOculusHMD* OculusHMD = GEngine->XRSystem.IsValid() ? (FOculusHMD*)(GEngine->XRSystem->GetHMDDevice()) : nullptr;
	if (!OculusHMD)
	{
		UE_LOG(LogMR, Warning, TEXT("Unable to retrieve OculusHMD"));
		return;
	}

	FPose CameraTrackedObjectPose;
	if (!GetCameraTrackedObjectPoseInTrackingSpace(OculusHMD, TrackedCamera, CameraTrackedObjectPose))
	{
		return;
	}

	FPose CameraPose = CameraTrackedObjectPose * FPose(TrackedCamera.CalibratedRotation.Quaternion(), TrackedCamera.CalibratedOffset);
	CameraPose = CameraPose * FPose(TrackedCamera.UserRotation.Quaternion(), TrackedCamera.UserOffset);

	float Distance = 0.0f;
	if (ClippingReference == EOculusMR_ClippingReference::CR_TrackingReference)
	{
		Distance = -FVector::DotProduct(CameraPose.Orientation.GetForwardVector().GetSafeNormal2D(), CameraPose.Position);
	}
	else if (ClippingReference == EOculusMR_ClippingReference::CR_Head)
	{
		FQuat HeadOrientation;
		FVector HeadPosition;
		OculusHMD->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HeadOrientation, HeadPosition);
		FVector HeadToCamera = HeadPosition - CameraPose.Position;
		Distance = FVector::DotProduct(CameraPose.Orientation.GetForwardVector().GetSafeNormal2D(), HeadToCamera);
	}
	else
	{
		checkNoEntry();
	}
	ClippingPlaneDistance = FMath::Max(Distance, GMinClipZ);

	FPose FinalPose;
	if (bFollowTrackingReference)
	{
		FQuat TROrientation;
		FVector TRLocation;
		FRotator TRRotation;
		if (!UOculusMRFunctionLibrary::GetTrackingReferenceLocationAndRotationInWorldSpace(TrackingReferenceComponent, TRLocation, TRRotation))
		{
			UE_LOG(LogMR, Warning, TEXT("Could not get player position"));
			return;
		}

		TROrientation = TRRotation.Quaternion();
		FinalPose = FPose(TROrientation, TRLocation) * CameraPose;
	}
	else
	{
		FPose CameraPoseOffset = FPose(InitialCameraRelativeOrientation, InitialCameraRelativePosition).Inverse() * CameraPose;
		FinalPose = FPose(InitialCameraAbsoluteOrientation, InitialCameraAbsolutePosition) * CameraPoseOffset;
	}

	FTransform FinalTransform(FinalPose.Orientation, FinalPose.Position);
	RootComponent->SetWorldTransform(FinalTransform);
	GetCameraComponent()->FieldOfView = TrackedCamera.FieldOfView;
}

void AOculusMR_CastingCameraActor::OpenCastingWindow()
{
	if (CastingWindowComponent->HasCastingWindowOpened()) return;

	if (!RefreshExternalCamera())
	{
		return;
	}

	RequestTrackedCameraCalibration();

	if (CompositionMethod == ECastingViewportCompositionMethod::DirectComposition)
	{
		ovrpBool available = ovrpBool_False;

		if (CapturingCamera == EOculusMR_CameraDeviceEnum::CD_None)
		{
			CurrentCapturingCamera = ovrpCameraDevice_None;
			UE_LOG(LogMR, Error, TEXT("CapturingCamera is set to CD_None which is invalid in DirectComposition. Please pick a valid camera for CapturingCamera. If you are not sure, try to set it to CD_WebCamera0 and use the first connected USB web camera"));
			return;
		}

		CurrentCapturingCamera = ConvertCameraDevice(CapturingCamera);
		if (OVRP_FAILURE(ovrp_IsCameraDeviceAvailable2(CurrentCapturingCamera, &available)) || !available)
		{
			CurrentCapturingCamera = ovrpCameraDevice_None;
			UE_LOG(LogMR, Error, TEXT("CapturingCamera not available"));
			return;
		}

		ovrpSizei Size;
		if (TrackedCamera.Index >= 0)
		{
			Size.w = TrackedCamera.SizeX;
			Size.h = TrackedCamera.SizeY;
			ovrp_SetCameraDevicePreferredColorFrameSize(CurrentCapturingCamera, Size);
		}
		else
		{
			Size.w = 1280;
			Size.h = 720;
			ovrp_SetCameraDevicePreferredColorFrameSize(CurrentCapturingCamera, Size);
		}

		if (bUseDynamicLighting)
		{
			ovrpBool supportDepth = ovrpBool_False;
			if (OVRP_SUCCESS(ovrp_DoesCameraDeviceSupportDepth(CurrentCapturingCamera, &supportDepth)) && supportDepth)
			{
				ovrp_SetCameraDeviceDepthSensingMode(CurrentCapturingCamera, ovrpCameraDeviceDepthSensingMode_Fill);
				ovrp_SetCameraDevicePreferredDepthQuality(CurrentCapturingCamera, ConvertCameraDepthQuality(DepthQuality));
			}
		}

		ovrpResult result = ovrp_OpenCameraDevice(CurrentCapturingCamera);

		if (OVRP_SUCCESS(result))
		{
			UE_LOG(LogMR, Log, TEXT("Create CameraColorTexture (1280x720)"));
			CameraColorTexture = UTexture2D::CreateTransient(1280, 720);
			CameraColorTexture->UpdateResource();
			CameraDepthTexture = DefaultTexture_White;

			if (CastingWindowComponent)
			{
				CastingWindowComponent->SetCameraColorTexture(CameraColorTexture);
			}
		}
		else
		{
			CurrentCapturingCamera = ovrpCameraDevice_None;
			UE_LOG(LogMR, Error, TEXT("Unable to open CapturingCamera"));
			return;
		}

		SetupCameraFrameMaterialInstance();
	}
	else if (CompositionMethod == ECastingViewportCompositionMethod::MultiView)
	{
		SetupBackdropMaterialInstance();
	}

	RepositionPlaneMesh();

	int ViewWidth = bUseTrackedCameraResolution ? TrackedCamera.SizeX : WidthPerView;
	int ViewHeight = bUseTrackedCameraResolution ? TrackedCamera.SizeY : HeightPerView;
	CastingWindowComponent->OpenCastingWindow(CompositionMethod, ViewWidth, ViewHeight);

	CastingWindowComponent->OnWindowClosedDelegate = FOculusMR_OnCastingWindowClosed::CreateLambda([this]() {
		if (CurrentCapturingCamera != ovrpCameraDevice_None)
		{
			ovrp_CloseCameraDevice(CurrentCapturingCamera);
			CurrentCapturingCamera = ovrpCameraDevice_None;
		}
		PlaneMeshComponent->SetVisibility(false);
		CameraFrameMaterialInstance = NULL;
		CastingWindowComponent->OnWindowClosedDelegate.Unbind();
	});
}

void AOculusMR_CastingCameraActor::SetupCameraFrameMaterialInstance()
{
	if (bUseDynamicLighting)
	{
		if (!ChromaKeyLitMaterialInstance && ChromaKeyLitMaterial)
		{
			ChromaKeyLitMaterialInstance = UMaterialInstanceDynamic::Create(ChromaKeyLitMaterial, this);
		}
		CameraFrameMaterialInstance = ChromaKeyLitMaterialInstance;
	}
	else
	{
		if (!ChromaKeyMaterialInstance && ChromaKeyMaterial)
		{
			ChromaKeyMaterialInstance = UMaterialInstanceDynamic::Create(ChromaKeyMaterial, this);
		}
		CameraFrameMaterialInstance = ChromaKeyMaterialInstance;
	}

	PlaneMeshComponent->SetMaterial(0, CameraFrameMaterialInstance);

	if (CameraFrameMaterialInstance)
	{
		CameraFrameMaterialInstance->SetTextureParameterValue(FName(TEXT("CameraCaptureTexture")), CameraColorTexture);
		CameraFrameMaterialInstance->SetVectorParameterValue(FName(TEXT("CameraCaptureTextureSize")),
			FLinearColor((float)CameraColorTexture->GetSizeX(), (float)CameraColorTexture->GetSizeY(), 1.0f / CameraColorTexture->GetSizeX(), 1.0f / CameraColorTexture->GetSizeY()));
		if (bUseDynamicLighting)
		{
			CameraFrameMaterialInstance->SetTextureParameterValue(FName(TEXT("CameraDepthTexture")), CameraDepthTexture);
		}
	}
}

void AOculusMR_CastingCameraActor::SetupBackdropMaterialInstance()
{
	if (!BackdropMaterialInstance && OpaqueColoredMaterial)
	{
		BackdropMaterialInstance = UMaterialInstanceDynamic::Create(OpaqueColoredMaterial, this);
	}
	PlaneMeshComponent->SetMaterial(0, BackdropMaterialInstance);
	if (BackdropMaterialInstance)
	{
		BackdropMaterialInstance->SetVectorParameterValue(FName(TEXT("Color")), GetForegroundLayerBackgroundColor());
	}
}

void AOculusMR_CastingCameraActor::RepositionPlaneMesh()
{
	FVector PlaneCenter = FVector::ForwardVector * ClippingPlaneDistance;
	FVector PlaneUp = FVector::UpVector;
	FVector PlaneNormal = -FVector::ForwardVector;
	int ViewWidth = bUseTrackedCameraResolution ? TrackedCamera.SizeX : WidthPerView;
	int ViewHeight = bUseTrackedCameraResolution ? TrackedCamera.SizeY : HeightPerView;
	float Width = ClippingPlaneDistance * FMath::Tan(FMath::DegreesToRadians(GetCameraComponent()->FieldOfView) * 0.5f) * 2.0f;
	float Height = Width * ViewHeight / ViewWidth;
	FVector2D PlaneSize = FVector2D(Width, Height);
	PlaneMeshComponent->Place(PlaneCenter, PlaneUp, PlaneNormal, PlaneSize);
	if (CameraFrameMaterialInstance && bUseDynamicLighting)
	{
		float WidthInMeter = Width / GWorld->GetWorldSettings()->WorldToMeters;
		float HeightInMeter = Height / GWorld->GetWorldSettings()->WorldToMeters;
		CameraFrameMaterialInstance->SetVectorParameterValue(FName(TEXT("TextureWorldSize")), FLinearColor(WidthInMeter, HeightInMeter, 1.0f / WidthInMeter, 1.0f / HeightInMeter));
	}
	PlaneMeshComponent->ResetRelativeTransform();
	PlaneMeshComponent->SetVisibility(true);
}

void AOculusMR_CastingCameraActor::OnHMDRecentered()
{
	RefreshBoundaryMesh();
}

void AOculusMR_CastingCameraActor::RefreshBoundaryMesh()
{
	RefreshBoundaryMeshCounter = 3;
}

void AOculusMR_CastingCameraActor::CloseCastingWindow()
{
	if (!CastingWindowComponent->HasCastingWindowOpened()) return;

	CastingWindowComponent->CloseCastingWindow();
}

void AOculusMR_CastingCameraActor::ToggleCastingWindow()
{
	if (HasCastingWindowOpened())
	{
		CloseCastingWindow();
	}
	else
	{
		OpenCastingWindow();
	}
}

bool AOculusMR_CastingCameraActor::HasCastingWindowOpened() const
{
	return CastingWindowComponent->HasCastingWindowOpened();
}

#undef LOCTEXT_NAMESPACE
