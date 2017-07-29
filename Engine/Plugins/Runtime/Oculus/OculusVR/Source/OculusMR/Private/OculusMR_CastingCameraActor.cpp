// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusMR_CastingCameraActor.h"

#include "OculusMRPrivate.h"
#include "OculusHMD_Settings.h"
#include "OculusHMD.h"
#include "OculusMR_CastingWindowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "OculusMR_PlaneMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Texture2D.h"
#include "RenderingThread.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraComponent.h"

#define LOCTEXT_NAMESPACE "OculusMR_CastingCameraActor"

static TAutoConsoleVariable<int32> CAutoOpenCastingVar(TEXT("mr.AutoOpenCasting"), 0, TEXT("Auto open casting: 1=MultiView; 2=DirectComposition"));
static TAutoConsoleVariable<float> CChromaTorelanceAVar(TEXT("mr.ChromaTorelanceA"), 20.0f, TEXT("Chroma Parameters"));
static TAutoConsoleVariable<float> CChromaTorelanceBVar(TEXT("mr.ChromaTorelanceB"), 15.0f, TEXT("Chroma Parameters"));
static TAutoConsoleVariable<float> CChromaShadowsVar(TEXT("mr.ChromaShadows"), 0.02f, TEXT("Chroma Parameters"));
static TAutoConsoleVariable<float> CChromaAlphaCutoffVar(TEXT("mr.ChromaAlphaCutoff"), 0.01f, TEXT("Chroma Parameters"));
static TAutoConsoleVariable<float> CCastingLantencyVar(TEXT("mr.CastingLantency"), 0, TEXT("Casting Latency"));

static TAutoConsoleVariable<int32> CProjectToMirrorWindowVar(TEXT("mr.ProjectToMirrorWindow"), 0, TEXT("Casting To MirrorWindow"));

namespace
{
	ovrpCameraDevice ConvertCameraDevice(EOculusMR_CameraDeviceEnum device)
	{
		if (device == EOculusMR_CameraDeviceEnum::CD_WebCamera0)
			return ovrpCameraDevice_WebCamera0;
		else if (device == EOculusMR_CameraDeviceEnum::CD_WebCamera1)
			return ovrpCameraDevice_WebCamera1;
		checkNoEntry();
		return ovrpCameraDevice_None;
	}

	bool GetTrackingReferenceLocationAndRotationInWorldSpace(USceneComponent* TrackingReferenceComponent, FVector& TRLocation, FRotator& TRRotation)
	{
		if (!TrackingReferenceComponent)
		{
			APlayerController* PlayerController = GWorld->GetFirstPlayerController();
			if (!PlayerController)
			{
				return false;
			}
			APawn* Pawn = PlayerController->GetPawn();
			if (!Pawn)
			{
				return false;
			}
			TRLocation = Pawn->GetActorLocation();
			TRRotation = Pawn->GetActorRotation();
			return true;
		}
		else
		{
			TRLocation = TrackingReferenceComponent->GetComponentLocation();
			TRRotation = TrackingReferenceComponent->GetComponentRotation();
			return true;
		}
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
			result = ovrp_GetNodePoseState2(ovrpStep_Game, deviceNode, &cameraPoseState);
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
	, ChromaTorelanceA(20.f)
	, ChromaTorelanceB(15.f)
	, ChromaShadows(0.02f)
	, ChromaAlphaCutoff(0.01f)
	, CurrentCapturingCamera(ovrpCameraDevice_None)
	, GreenScreenMaterial(NULL)
	, GreenScreenMaterialInstance(NULL)
	, TrackedCameraCalibrationRequired(false)
	, HasTrackedCameraCalibrationCalibrated(false)
{
	CastingWindowComponent = CreateDefaultSubobject<UOculusMR_CastingWindowComponent>(TEXT("OutputWindowComponent"));
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;

	PlaneMeshComponent = CreateDefaultSubobject<UOculusMR_PlaneMeshComponent>(TEXT("PlaneMeshComponent"));
	PlaneMeshComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	PlaneMeshComponent->ResetRelativeTransform();
	PlaneMeshComponent->SetVisibility(false);

	GreenScreenMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/OculusVR/OculusMR_GreenKey")));
	if (!GreenScreenMaterial)
	{
		UE_LOG(LogMR, Warning, TEXT("Invalid GreenScreenMaterial"));
	}
	LoadFromIni();
}

void AOculusMR_CastingCameraActor::LoadFromIni()
{
	const TCHAR* OculusMRSettings = TEXT("Oculus.MixedReality.CastingSettings");
	bool v;
	float f;
	int i;
	FVector vec;
	FColor color;
	if (GConfig->GetBool(OculusMRSettings, TEXT("UseTrackedCameraResolution"), v, GEngineIni))
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
	if (GConfig->GetFloat(OculusMRSettings, TEXT("CastingLatency"), f, GEngineIni))
	{
		CastingLatency = f;
	}
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
		FOculusHMD* OculusHMD = (FOculusHMD*)(GEngine->HMDDevice.Get());
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
}

void AOculusMR_CastingCameraActor::EndPlay(EEndPlayReason::Type Reason)
{
	CloseCastingWindow();
	Super::EndPlay(Reason);
}

void AOculusMR_CastingCameraActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasCastingWindowOpened())
	{
		if (!RefreshExternalCamera())
		{
			CloseCastingWindow();
			return;
		}

		if (CChromaTorelanceAVar.GetValueOnAnyThread() > 0.0f)
		{
			ChromaTorelanceA = CChromaTorelanceAVar.GetValueOnAnyThread();
		}

		if (CChromaTorelanceBVar.GetValueOnAnyThread() > 0.0f)
		{
			ChromaTorelanceB = CChromaTorelanceBVar.GetValueOnAnyThread();
		}

		if (CChromaShadowsVar.GetValueOnAnyThread() > 0.0f)
		{
			ChromaShadows = CChromaShadowsVar.GetValueOnAnyThread();
		}

		if (CChromaAlphaCutoffVar.GetValueOnAnyThread() > 0.0f)
		{
			ChromaAlphaCutoff = CChromaAlphaCutoffVar.GetValueOnAnyThread();
		}

		if (CCastingLantencyVar.GetValueOnAnyThread() > 0.0f)
		{
			CastingLatency = CCastingLantencyVar.GetValueOnAnyThread();
		}

		CastingWindowComponent->SetExpectedLantencyInSeconds(CastingLatency);
		if (GreenScreenMaterialInstance)
		{
			GreenScreenMaterialInstance->SetScalarParameterValue(FName(TEXT("ChromaTorelanceA")), ChromaTorelanceA);
			GreenScreenMaterialInstance->SetScalarParameterValue(FName(TEXT("ChromaTorelanceB")), ChromaTorelanceB);
			GreenScreenMaterialInstance->SetScalarParameterValue(FName(TEXT("ChromaShadows")), ChromaShadows);
			GreenScreenMaterialInstance->SetScalarParameterValue(FName(TEXT("ChromaAlphaCutoff")), ChromaAlphaCutoff);
		}
		if (CurrentCapturingCamera != ovrpCameraDevice_None)
		{
			ovrpBool available = ovrpBool_False;
			ovrpSizei colorFrameSize = { 0, 0 };
			const ovrpByte* colorFrameData = nullptr;
			int rowPitch = 0;

			if (OVRP_SUCCESS(ovrp_IsCameraDeviceColorFrameAvailable2(CurrentCapturingCamera, &available)) && available &&
				OVRP_SUCCESS(ovrp_GetCameraDeviceColorFrameSize(CurrentCapturingCamera, &colorFrameSize)) &&
				OVRP_SUCCESS(ovrp_GetCameraDeviceColorFrameBgraPixels(CurrentCapturingCamera, &colorFrameData, &rowPitch)))
			{
				UpdateCameraColorTexture(colorFrameSize, colorFrameData, rowPitch);
			}
		}
		if (TrackedCameraCalibrationRequired)
		{
			CalibrateTrackedCameraPose();
		}
		UpdateTrackedCameraPosition();
		if (CompositionMethod == ECastingViewportCompositionMethod::DirectComposition)
		{
			RepositionPlaneMesh();
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
		if (GreenScreenMaterialInstance)
		{
			GreenScreenMaterialInstance->SetTextureParameterValue(FName(TEXT("CameraCaptureTexture")), CameraColorTexture);
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

void AOculusMR_CastingCameraActor::BindToTrackedCameraIndexIfAvailable(int InTrackedCameraIndex)
{
	FTrackedCamera TempTrackedCamera;
	if (InTrackedCameraIndex >= 0)
	{
		TArray<FTrackedCamera> TrackedCameras;
		UOculusMRFunctionLibrary::GetAllTrackedCamera(TrackedCameras);
		for (int i = 0; i < TrackedCameras.Num(); ++i)
		{
			if (TrackedCameras[i].Index == InTrackedCameraIndex)
			{
				TempTrackedCamera = TrackedCameras[i];
				break;
			}
		}
	}
	TrackedCamera = TempTrackedCamera;
	if (TrackedCamera.Index < 0)
	{
		SetTrackedCameraUserPoseWithCameraTransform();
	}
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

	FOculusHMD* OculusHMD = (FOculusHMD*)(GEngine->HMDDevice.Get());
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
	if (!GetTrackingReferenceLocationAndRotationInWorldSpace(TrackingReferenceComponent, TRLocation, TRRotation))
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

	FOculusHMD* OculusHMD = (FOculusHMD*)(GEngine->HMDDevice.Get());
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
	if (!GetTrackingReferenceLocationAndRotationInWorldSpace(TrackingReferenceComponent, TRLocation, TRRotation))
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

	FOculusHMD* OculusHMD = (FOculusHMD*)(GEngine->HMDDevice.Get());
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
		FPose HeadPose = OculusHMD->GetFrame()->HeadPose;
		FVector HeadToCamera = HeadPose.Position - CameraPose.Position;
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
		if (!GetTrackingReferenceLocationAndRotationInWorldSpace(TrackingReferenceComponent, TRLocation, TRRotation))
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

		GreenScreenMaterialInstance = UMaterialInstanceDynamic::Create(GreenScreenMaterial, this);
		PlaneMeshComponent->SetMaterial(0, GreenScreenMaterialInstance);

		ovrpResult result = ovrp_OpenCameraDevice(CurrentCapturingCamera);

		if (OVRP_SUCCESS(result))
		{
			UE_LOG(LogMR, Log, TEXT("Create CameraColorTexture (1280x720)"));
			CameraColorTexture = UTexture2D::CreateTransient(1280, 720);
			CameraColorTexture->UpdateResource();
			if (GreenScreenMaterialInstance)
			{
				GreenScreenMaterialInstance->SetTextureParameterValue(FName(TEXT("CameraCaptureTexture")), CameraColorTexture);
			}
			if (CastingWindowComponent)
			{
				CastingWindowComponent->SetCameraColorTexture(CameraColorTexture);
			}
			if (CompositionMethod == ECastingViewportCompositionMethod::DirectComposition)
			{
				RepositionPlaneMesh();
			}
		}
		else
		{
			CurrentCapturingCamera = ovrpCameraDevice_None;
			UE_LOG(LogMR, Error, TEXT("Unable to open CapturingCamera"));
			return;
		}
	}
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
		GreenScreenMaterialInstance = NULL;
		CastingWindowComponent->OnWindowClosedDelegate.Unbind();
	});
}

void AOculusMR_CastingCameraActor::RepositionPlaneMesh()
{
	if (CameraColorTexture)
	{
		FVector PlaneCenter = FVector::ForwardVector * ClippingPlaneDistance;
		FVector PlaneUp = FVector::UpVector;
		FVector PlaneNormal = -FVector::ForwardVector;
		float Width = ClippingPlaneDistance * FMath::Tan(FMath::DegreesToRadians(GetCameraComponent()->FieldOfView) * 0.5f) * 2.0f;
		float Height = Width * (float)CameraColorTexture->GetSizeY() / (float)CameraColorTexture->GetSizeX();
		FVector2D PlaneSize = FVector2D(Width, Height);
		PlaneMeshComponent->Place(PlaneCenter, PlaneUp, PlaneNormal, PlaneSize);
		PlaneMeshComponent->ResetRelativeTransform();
		PlaneMeshComponent->SetVisibility(true);
	}
	else
	{
		PlaneMeshComponent->SetVisibility(false);
	}
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
