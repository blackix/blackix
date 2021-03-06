// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Slate.h"
#include "TaskGraphInterfaces.h"
#include "VisualizerEvents.h"
#include "STaskGraph.h"
#include "SGraphBar.h"
#include "Tickable.h"
#include "SProfileVisualizer.h"

/**
 * Creates Visualizer using Visualizer profile data format
 *
 * @param ProfileData Visualizer data
 * @return Visualizer window
 */
void MakeTaskGraphVisualizerWindow( TSharedPtr< FVisualizerEvent > ProfileData, const FText& WindowTitle, const FText& ProfilerType )
{
	FGlobalTabmanager::Get()->InsertNewDocumentTab
		(
			"VisualizerSpawnPoint", FTabManager::ESearchPreference::RequireClosedTab,
			SNew( SDockTab )
			.Label( WindowTitle )
			.TabRole( ETabRole::DocumentTab )
			[
				SNew( SProfileVisualizer )
				.ProfileData( ProfileData )
				.ProfilerType( ProfilerType )
			]
		);
}

/** Helper class that counts down when to unpause and stop movie. */
class FDelayedVisualizerSpawner : public FTickableGameObject
{
public:

	struct FPendingWindow
	{
		FText Title;
		FText Type;
		TSharedPtr< FVisualizerEvent > ProfileData;

		FPendingWindow( TSharedPtr<FVisualizerEvent> InData, const FText& InTitle, const FText& InType )
			: Title( InTitle )
			, Type( InType )
			, ProfileData( InData )
		{}
	};

	FDelayedVisualizerSpawner( )
	{
	}

	virtual ~FDelayedVisualizerSpawner()
	{
	}

	void AddPendingData( TSharedPtr<FVisualizerEvent> InProfileData, const FText& InTitle, const FText& InType )
	{
		DataLock.Lock();

		VisualizerDataToSpawn.Add( MakeShareable( new FPendingWindow( InProfileData, InTitle, InType ) )  );

		DataLock.Unlock();
	}

	// FTickableGameObject interface

	void Tick(float DeltaTime)
	{
		DataLock.Lock();

		for ( int32 Index = 0; Index < VisualizerDataToSpawn.Num(); Index++ )
		{
			TSharedPtr<FPendingWindow> Window =  VisualizerDataToSpawn[ Index ];
			MakeTaskGraphVisualizerWindow( Window->ProfileData, Window->Title, Window->Type ); 
		}

		VisualizerDataToSpawn.Empty();

		DataLock.Unlock();
	}

	/** We should call Tick on this object */
	virtual bool IsTickable() const
	{
		return true;
	}

	/** Need this to be ticked when paused (that is the point!) */
	virtual bool IsTickableWhenPaused() const
	{
		return true;
	}

	virtual TStatId GetStatId() const OVERRIDE
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDelayedVisualizerSpawner, STATGROUP_Tickables);
	}

private:

	FCriticalSection DataLock;

	/** List of profile data sets to spawn visualizers for */
	TArray< TSharedPtr<FPendingWindow> > VisualizerDataToSpawn;
};

static TSharedPtr< FDelayedVisualizerSpawner > GDelayedVisualizerSpawner;

void InitProfileVisualizer()
{
	if( GDelayedVisualizerSpawner.IsValid() == false )
	{
		GDelayedVisualizerSpawner = MakeShareable( new FDelayedVisualizerSpawner() );
	}
}

void ShutdownProfileVisualizer()
{
	GDelayedVisualizerSpawner.Reset();
}

static bool GHasRegisteredVisualizerLayout = false;

void DisplayProfileVisualizer( TSharedPtr< FVisualizerEvent > InProfileData, const TCHAR* InProfilerType )
{
	check( IsInGameThread() );

	if ( !GHasRegisteredVisualizerLayout )
	{
		TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout( "Visualizer_Layout" )
		->AddArea
		(
			FTabManager::NewArea(720, 768)
			->Split
			(
				FTabManager::NewStack()
				->AddTab("VisualizerSpawnPoint", ETabState::ClosedTab)
			)
		);

		FGlobalTabmanager::Get()->RestoreFrom( Layout, TSharedPtr<SWindow>() );
		GHasRegisteredVisualizerLayout = true;
	}

	FFormatNamedArguments Args;
	Args.Add( TEXT("ProfilerType"), FText::FromString( InProfilerType ) );

	const FText WindowTitle = FText::Format( NSLOCTEXT("TaskGraph", "WindowTitle", "{ProfilerType} Visualizer"), Args );
	const FText ProfilerType = FText::Format( NSLOCTEXT("TaskGraph", "ProfilerType", "{ProfilerType} Profile"), Args );


	MakeTaskGraphVisualizerWindow( InProfileData, WindowTitle, ProfilerType );
	
}

/**
 * Module for profile visualizer.
 */
class FProfileVisualizerModule : public IProfileVisualizerModule
{
public:
	virtual void StartupModule() OVERRIDE
	{
		::InitProfileVisualizer();
	}
	virtual void ShutdownModule() OVERRIDE
	{
		::ShutdownProfileVisualizer();
	}
	virtual void DisplayProfileVisualizer( TSharedPtr< FVisualizerEvent > InProfileData, const TCHAR* InProfilerType ) OVERRIDE
	{
		::DisplayProfileVisualizer( InProfileData, InProfilerType );
	}
};
IMPLEMENT_MODULE(FProfileVisualizerModule, TaskGraph);
