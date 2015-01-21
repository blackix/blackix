// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISourceControlProvider.h"
#include "IPerforceSourceControlWorker.h"
#include "PerforceSourceControlState.h"

DECLARE_DELEGATE_RetVal(FPerforceSourceControlWorkerRef, FGetPerforceSourceControlWorker)

// We disable the deprecation warnings here because otherwise it'll complain about us
// implementing RegisterSourceControlStateChanged and UnregisterSourceControlStateChanged.  We know
// that, but we only want it to complain if *others* implement or call these functions.
//
// These macros should be removed when those functions are removed.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FPerforceSourceControlProvider : public ISourceControlProvider
{
public:
	/** Constructor */
	FPerforceSourceControlProvider()
		: bServerAvailable(false)
		, PersistentConnection(NULL)
#if PLATFORM_WINDOWS
		, Module_libeay32(NULL)
		, Module_ssleay32(NULL)
#endif
	{
	}

	/* ISourceControlProvider implementation */
	virtual void Init(bool bForceConnection = true) override;
	virtual void Close() override;
	virtual FText GetStatusText() const override;
	virtual bool IsEnabled() const override;
	virtual bool IsAvailable() const override;
	virtual const FName& GetName(void) const override;
	virtual ECommandResult::Type GetState( const TArray<FString>& InFiles, TArray< TSharedRef<ISourceControlState, ESPMode::ThreadSafe> >& OutState, EStateCacheUsage::Type InStateCacheUsage ) override;
	virtual void RegisterSourceControlStateChanged( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) override;
	virtual void UnregisterSourceControlStateChanged( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) override;
	virtual FDelegateHandle RegisterSourceControlStateChanged_Handle( const FSourceControlStateChanged::FDelegate& SourceControlStateChanged ) override;
	virtual void UnregisterSourceControlStateChanged_Handle( FDelegateHandle Handle ) override;
	virtual ECommandResult::Type Execute( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation, const TArray<FString>& InFiles, EConcurrency::Type InConcurrency = EConcurrency::Synchronous, const FSourceControlOperationComplete& InOperationCompleteDelegate = FSourceControlOperationComplete() ) override;
	virtual bool CanCancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) const override;
	virtual void CancelOperation( const TSharedRef<ISourceControlOperation, ESPMode::ThreadSafe>& InOperation ) override;
	virtual bool UsesLocalReadOnlyState() const override;
	virtual bool UsesChangelists() const override;
	virtual void Tick() override;
	virtual TArray< TSharedRef<class ISourceControlLabel> > GetLabels( const FString& InMatchingSpec ) const override;
#if SOURCE_CONTROL_WITH_SLATE
	virtual TSharedRef<class SWidget> MakeSettingsWidget() const override;
#endif

	/**
	 * Register a worker with the provider.
	 * This is used internally so the provider can maintain a map of all available operations.
	 */
	void RegisterWorker( const FName& InName, const FGetPerforceSourceControlWorker& InDelegate );

	/**
	 * Gets a list of client spec names from the source control provider
	 *
	 * @param	InConnectionInfo	Credentials for connection
	 * @param	OutWorkspaceList	List of client spec name strings
	 * @param	OutErrorMessages	List of any error messages that may have occurred
	 */
	void GetWorkspaceList(const struct FPerforceConnectionInfo& InConnectionInfo, TArray<FString>& OutWorkspaceList, TArray<FText>& OutErrorMessages);

	/** Get the P4 ticket we will use for connections */
	const FString& GetTicket() const;

	/** Helper function used to update state cache */
	TSharedRef<FPerforceSourceControlState, ESPMode::ThreadSafe> GetStateInternal(const FString& InFilename);

	/**
	 * Connects to the source control server if the persistent connection is not already established.
	 *
	 * @return true if the connection is established or became established and false if the connection failed.
	 */
	bool EstablishPersistentConnection();

	/** Get the persistent connection, if any */
	class FPerforceConnection* GetPersistentConnection()
	{
		return PersistentConnection;
	}

private:

	/** Helper function used to create a worker for a particular operation */
	TSharedPtr<class IPerforceSourceControlWorker, ESPMode::ThreadSafe> CreateWorker(const FName& InOperationName) const;

	/** 
	 * Logs any messages that a command needs to output.
	 */
	void OutputCommandMessages(const class FPerforceSourceControlCommand& InCommand) const;

	/**
	 * Loads user/SCC information from the INI file.
	 */
	void ParseCommandLineSettings(bool bForceConnection);

	/**
	 * Helper function for running command 'synchronously'.
	 * This really doesn't execute synchronously; rather it adds the command to the queue & does not return until
	 * the command is completed.
	 */
	ECommandResult::Type ExecuteSynchronousCommand(class FPerforceSourceControlCommand& InCommand, const FText& Task, bool bSuppressResponseMsg);

	/**
	 * Run a command synchronously or asynchronously.
	 */
	ECommandResult::Type IssueCommand(class FPerforceSourceControlCommand& InCommand, const bool bSynchronous);

	/**
	 * Load the OpenSSL libraries needed to support SSL (currently windows only)
	 */
	void LoadSSLLibraries();

	/**
	 * Unload the OpenSSL libraries needed to support SSL (currently windows only)
	 */
	void UnloadSSLLibraries();

private:

	/** The ticket we use for login. */
	FString Ticket;

	/** the root of the workspace we are currently using */
	FString WorkspaceRoot;

	/** Indicates if source control integration is available or not. */
	bool bServerAvailable;

	/** A pointer to the persistent P4 connection for synchronous operations */
	class FPerforceConnection* PersistentConnection;

#if PLATFORM_WINDOWS
	/** Module handles for OpenSSL dlls */
	HMODULE Module_libeay32;
	HMODULE Module_ssleay32;
#endif

	/** State cache */
	TMap<FString, TSharedRef<class FPerforceSourceControlState, ESPMode::ThreadSafe> > StateCache;

	/** The currently registered source control operations */
	TMap<FName, FGetPerforceSourceControlWorker> WorkersMap;

	/** Queue for commands given by the main thread */
	TArray < FPerforceSourceControlCommand* > CommandQueue;

	/** For notifying when the source control states in the cache have changed */
	FSourceControlStateChanged OnSourceControlStateChanged;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
