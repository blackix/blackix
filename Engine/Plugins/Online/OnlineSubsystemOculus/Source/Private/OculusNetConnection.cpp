// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OculusNetConnection.h"
#include "OnlineSubsystemOculusPrivate.h"


#include "IPAddressOculus.h"

void UOculusNetConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		/* PacketOverhead */ 1);

	// We handle our own overhead
	PacketOverhead = 0;

	// Initalize the send buffer
	InitSendBuffer();
}

void UOculusNetConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		0);
}

void UOculusNetConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState,
		// Use the default packet size/overhead unless overridden by a child class
		InMaxPacket == 0 ? MAX_PACKET_SIZE : InMaxPacket,
		0);

	auto OculusAddr = static_cast<const FInternetAddrOculus&>(InRemoteAddr);
	PeerID = OculusAddr.GetID();
}

void UOculusNetConnection::LowLevelSend(void* Data, int32 CountBytes, int32 CountBits)
{
	check(PeerID);
	const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

	UE_LOG(LogNetTraffic, VeryVerbose, TEXT("Low level send to: %llu Count: %d"), PeerID, CountBytes);

	// Process any packet modifiers
	if (Handler.IsValid() && !Handler->GetRawSend())
	{
		const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits);

		if (!ProcessedData.bError)
		{
			DataToSend = ProcessedData.Data;
			CountBytes = FMath::DivideAndRoundUp(ProcessedData.CountBits, 8);
		}
		else
		{
			CountBytes = 0;
		}
	}

	if (CountBytes > 0)
	{
		ovr_Net_SendPacket(PeerID, static_cast<size_t>(CountBytes), DataToSend, (InternalAck) ? ovrSend_Reliable : ovrSend_Unreliable);
	}
}

FString UOculusNetConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return FString::Printf(TEXT("%llu.oculus"), PeerID);
}

FString UOculusNetConnection::LowLevelDescribe()
{
	return FString::Printf(TEXT("PeerId=%llu"), PeerID);
}

void UOculusNetConnection::FinishDestroy()
{
  // Keep track if it's this call that is closing the connection before cleanup is called
  const bool bIsClosingOpenConnection = State != EConnectionState::USOCK_Closed;
	Super::FinishDestroy();

  // If this connection was open, then close it
	if (PeerID != 0 && bIsClosingOpenConnection)
	{
		ovr_Net_Close(PeerID);
	}
}

FString UOculusNetConnection::RemoteAddressToString()
{
	return LowLevelGetRemoteAddress(/* bAppendPort */ false);
}
