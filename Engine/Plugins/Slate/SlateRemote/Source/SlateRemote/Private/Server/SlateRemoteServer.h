// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * Implements a server that listens to events from the Slate Remote iOS application.
 */
class FSlateRemoteServer
{
public:

	/**
	 * Default constructor.
	 *
	 * @param InSocketSubsystem The socket subsystem to use.
	 * @param InServerEndpoint The server's network endpoint to listen on.
	 */
	FSlateRemoteServer( ISocketSubsystem& InSocketSubsystem, const FIPv4Endpoint& InServerEndpoint );

	/**
	 * Destructor.
	 */
	~FSlateRemoteServer( );

public:

	/**
	 * Starts the server.
	 *
	 * @param ServerEndpoint The network endpoint to listen on.
	 * @return true if the server was started, false otherwise.
	 */
	bool StartServer( const FIPv4Endpoint& ServerEndpoint );

	/**
	 * Stops the server.
	 */
	void StopServer( );

protected:

	/**
	 * Processes a DT_Gyro message.
	 *
	 * @param Message The message to process.
	 */
	void ProcessGyroMessage( const FSlateRemoteServerMessage& Message );

	/**
	 * Processes a DT_Motion message.
	 *
	 * @param Message The message to process.
	 */
	void ProcessMotionMessage( const FSlateRemoteServerMessage& Message );

	/**
	 * Processes a DT_Ping message.
	 *
	 * @param Message The message to process.
	 */
	void ProcessPingMessage( const FSlateRemoteServerMessage& Message );

	/**
	 * Processes a DT_Tilt message.
	 *
	 * @param Message The message to process.
	 */
	void ProcessTiltMessage( const FSlateRemoteServerMessage& Message );

	/**
	 * Processes a DT_Touch message.
	 *
	 * @param Message The message to process.
	 */
	void ProcessTouchMessage( const FSlateRemoteServerMessage& Message );

private:

	// Callback for when the ticker fires.
	bool HandleTicker( float DeltaTime );

private:

	// The widget path to the game viewport.
	FWeakWidgetPath GameViewportWidgetPath;

	// Highest message ID (must handle wrapping around at 16 bits).
	uint16 HighestMessageReceived;

	// The socket to send image data on, will be initialized in first tick.
	FSocket* ImageSocket;

	// The address of the most recent UDKRemote to talk to us, this is who we reply to.
	TSharedRef<FInternetAddr> ReplyAddr;

	// The socket to listen on, will be initialized in first tick.
	FSocket* ServerSocket;

	// Holds a pointer to the socket sub-system being used.
	ISocketSubsystem& SocketSubsystem;

	// Holds a delegate to be invoked when the server ticks.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FDelegateHandle TickDelegateHandle;

	// The time since the last message was received from the Slate Remote application.
	float TimeSinceLastPing;

	// Ever increasing timestamp to send to the input system.
	double Timestamp;
};
