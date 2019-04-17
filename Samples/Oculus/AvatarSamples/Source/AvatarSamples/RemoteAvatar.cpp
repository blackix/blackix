#include "AvatarSamples.h"
#include "RemoteAvatar.h"
#include "OvrAvatarManager.h"
#include "Object.h"
#include "OvrAvatar.h"

ARemoteAvatar::ARemoteAvatar()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RemoteAvatarRoot"));
	AvatarComponent = CreateDefaultSubobject<UOvrAvatar>(TEXT("RemoteAvatar"));


	PrimaryActorTick.bCanEverTick = true;
	PacketKey = GetName();
}

void ARemoteAvatar::BeginPlay()	
{
	Super::BeginPlay();
	SetActorHiddenInGame(true);

#if PLATFORM_ANDROID
	ovrAvatarAssetLevelOfDetail lod = ovrAvatarAssetLevelOfDetail_Three;
#else
	ovrAvatarAssetLevelOfDetail lod = ovrAvatarAssetLevelOfDetail_Five;
#endif
	AvatarComponent->RequestAvatar(FCString::Strtoui64(*OculusUserId, NULL, 10), lod, UseCombinedMesh);
	UOvrAvatarManager::Get().RegisterRemoteAvatar(PacketKey);
}

void ARemoteAvatar::BeginDestroy()
{
	Super::BeginDestroy();

	UOvrAvatarManager::Get().UnregisterRemoteAvatar(PacketKey);
}

void ARemoteAvatar::PreInitializeComponents()
{
	AvatarComponent->SetPlayerType(UOvrAvatar::ePlayerType::Remote);
	AvatarComponent->SetVisibilityType(ovrAvatarVisibilityFlag_ThirdPerson);
	AvatarComponent->SetExpressiveCapability(EnableExpressive);
	AvatarComponent->SetBodyCapability(EnableBody);
	AvatarComponent->SetHandsCapability(EnableHands);
	AvatarComponent->SetBaseCapability(EnableBase);
	AvatarComponent->SetBodyMaterial(ALocalAvatar::GetOvrAvatarMaterialFromType(BodyMaterial));
	AvatarComponent->SetHandMaterial(ALocalAvatar::GetOvrAvatarMaterialFromType(HandsMaterial));
}

void ARemoteAvatar::Tick(float DeltaSeconds)
{
	LatencyTick += DeltaSeconds;

	if (!CurrentPacket && LatencyTick > FakeLatency)
	{
		CurrentPacket = UOvrAvatarManager::Get().RequestAvatarPacket(PacketKey);
		SetActorHiddenInGame(false);
	}

	if (CurrentPacket)
	{
		const float PacketLength = UOvrAvatarManager::Get().GetSDKPacketDuration(CurrentPacket);
		AvatarComponent->UpdateFromPacket(CurrentPacket, FMath::Min(PacketLength, CurrentPacketTime));
		CurrentPacketTime += DeltaSeconds;

		if (CurrentPacketTime > PacketLength)
		{
			UOvrAvatarManager::Get().FreeSDKPacket(CurrentPacket);
			CurrentPacket = nullptr;
			CurrentPacketTime = CurrentPacketTime - PacketLength;
		}
	}
}

