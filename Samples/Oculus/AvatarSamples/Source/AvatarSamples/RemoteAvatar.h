#pragma once

#include "AvatarSamples.h"
#include "GameFramework/Pawn.h"
#include "OVR_Avatar.h"
#include "LocalAvatar.h"

#include "RemoteAvatar.generated.h"

class UOvrAvatar;

UCLASS()
class AVATARSAMPLES_API ARemoteAvatar : public APawn
{
	GENERATED_BODY()
public:
	ARemoteAvatar();

	void BeginPlay() override;
	void BeginDestroy() override;
	void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Avatar|Materials")
		AvatarMaterial BodyMaterial = AvatarMaterial::Masked;

	UPROPERTY(EditAnywhere, Category = "Avatar|Materials")
		AvatarMaterial HandsMaterial = AvatarMaterial::Translucent;

	UPROPERTY(EditAnywhere, Category = "Avatar")
		FString OculusUserId;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
		bool EnableExpressive = true;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
		bool EnableBody = true;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
		bool EnableHands = true;

	UPROPERTY(EditAnywhere, Category = "Avatar|Capabilities")
		bool EnableBase = true;

	UPROPERTY(EditAnywhere, Category = "Avatar")
		bool UseCombinedMesh = false;

	virtual void PreInitializeComponents() override;
private:
	UOvrAvatar* AvatarComponent = nullptr;
	ovrAvatarPacket* CurrentPacket = nullptr;

	float CurrentPacketTime = 0.f;
	float FakeLatency = 0.5f;
	float LatencyTick = 0.f;

	FString PacketKey;
};
