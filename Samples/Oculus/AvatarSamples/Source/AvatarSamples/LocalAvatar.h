#pragma once

#include "Online.h"
#include "GameFramework/Pawn.h"
#include "OvrAvatar.h"
#include <unordered_map>

#include "LocalAvatar.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAvatarSamples, Log, All);


UENUM()
enum class AvatarVisibility : uint8 {
	FirstPerson = 1 << 0, ///< Visible in the first person view
	ThirdPerson = 1 << 1, ///< Visible in the third person view
};

UENUM()
enum class AvatarMaterial : uint8 {
	Opaque,
	Translucent,
	Masked
};

UENUM()
enum class AvatarLevelOfDetail : uint8 {
	Low,
	Mid,
	High
};

struct AvatarLevelOfDetailHash
{
	template <typename T>
	std::size_t operator()(T t) const
	{
		return static_cast<std::size_t>(t);
	}
};


UCLASS()
class AVATARSAMPLES_API ALocalAvatar : public APawn
{
	GENERATED_BODY()
public:
	ALocalAvatar();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void SetupPlayerInputComponent(UInputComponent* Input) override;


	UPROPERTY(EditAnywhere, Category = "Avatar")
	bool UseLocalMicrophone = false;

	//Maps First/Third Person Visibility of Avatar
	UPROPERTY(EditAnywhere, Category = "Avatar")
	AvatarVisibility AvatarVisibilityType = AvatarVisibility::FirstPerson;

	UPROPERTY(EditAnywhere, Category = "Avatar|Materials")
	AvatarMaterial BodyMaterial = AvatarMaterial::Masked;

	UPROPERTY(EditAnywhere, Category = "Avatar|Materials")
	AvatarMaterial HandsMaterial = AvatarMaterial::Translucent;

	UPROPERTY(EditAnywhere, Category="Avatar" )
	FString OculusUserId;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
	bool EnableExpressive = true;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
	bool EnableBody = true;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
	bool EnableHands = true;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
	bool EnableBase = true;

	UPROPERTY(EditAnywhere, Category="Avatar")
	bool UseCombinedMesh = false;

	UPROPERTY(EditAnywhere, Category = "Avatar")
	AvatarLevelOfDetail LevelOfDetail = AvatarLevelOfDetail::High;

	virtual void PreInitializeComponents() override;
	static UOvrAvatar::MaterialType GetOvrAvatarMaterialFromType(AvatarMaterial material);
private:
		
	void UpdatePacketRecording(float DeltaTime);

	void CycleRightHandPose();
	void CycleLeftHandPose();

	UFUNCTION()
	void LipSyncVismesReady();

	FDelegateHandle OnLoginCompleteDelegateHandle;

	enum class eHandPoseState
	{
		Default,
		Sphere,
		Cube,
		Custom,
		Controller,
		Detached,
	};

	eHandPoseState LeftHandPoseIndex = eHandPoseState::Default;
	eHandPoseState RightHandPoseIndex = eHandPoseState::Default;

	UOvrAvatar* AvatarComponent = nullptr;
	class UOVRLipSyncPlaybackActorComponent* PlayBackLipSyncComponent = nullptr;
	class UOVRLipSyncActorComponent* LipSyncComponent = nullptr;
	class UAudioComponent* AudioComponent = nullptr;

	struct FPacketRecordSettings
	{
		bool Initialized = false;
		bool RecordingFrames = false;
		float UpdateRate = 1.0f / 45.f;
		float AccumulatedTime = 0.f;
	};

	FPacketRecordSettings PacketSettings;

	bool UseCannedLipSyncPlayback = false;

	static std::unordered_map<AvatarLevelOfDetail, ovrAvatarAssetLevelOfDetail, AvatarLevelOfDetailHash> LODMap;
};
