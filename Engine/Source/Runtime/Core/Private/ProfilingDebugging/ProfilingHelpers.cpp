// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

/**
 * Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
 * code everywhere.  And we can have consistent naming for all our files.
 *
 */

// Core includes.
#include "CorePrivate.h"
#include "EngineVersion.h"


// find where these are really defined
const static int32 MaxFilenameLen = 100;

#if WITH_ENGINE
FGetMapNameDelegate GGetMapNameDelegate;
#endif

/**
 * This will get the changelist that should be used with the Automated Performance Testing
 * If one is passed in we use that otherwise we use the GEngineVersion.GetChangelist().  This allows
 * us to have build machine built .exe and test them. 
 *
 * NOTE: had to use AutomatedBenchmarking as the parsing code is flawed and doesn't match
 *       on whole words.  so automatedperftestingChangelist was failing :-( 
 **/
int32 GetChangeListNumberForPerfTesting()
{
	int32 Retval = GEngineVersion.GetChangelist();

	// if we have passed in the changelist to use then use it
	int32 FromCommandLine = 0;
	FParse::Value( FCommandLine::Get(), TEXT("-gABC="), FromCommandLine );

	// we check for 0 here as the CIS always appends -AutomatedPerfChangelist but if it
	// is from the "built" builds when it will be a 0
	if( FromCommandLine != 0 )
	{
		Retval = FromCommandLine;
	}

	return Retval;
}

/**
 * This makes it so UnrealConsole will open up the memory profiler for us
 *
 * @param NotifyType has the <namespace>:<type> (e.g. UE_PROFILER!UE3STATS:)
 * @param FullFileName the File name to copy from the console
 **/
void SendDataToPCViaUnrealConsole( const FString& NotifyType, const FString& FullFileName )
{
	//UE_LOG(LogProfilingDebugging, Warning, TEXT("SendDataToPCViaUnrealConsole %s%s"), *NotifyType, *FullFileName );

	const FString NotifyString = NotifyType + FullFileName;
	
	// send it across via UnrealConsole
	FMsg::SendNotificationString( *NotifyString );
}



/** 
 * This will generate the profiling file name that will work with limited filename sizes on consoles.
 * We want a uniform naming convention so we will all just call this function.
 *
 * @param ProfilingType this is the type of profiling file this is
 * 
 **/
FString CreateProfileFilename( const FString& InFileExtension, bool bIncludeDateForDirectoryName )
{
	FString Retval;

	// set up all of the parts we will use
	FString MapNameStr;

#if WITH_ENGINE
	checkf(GGetMapNameDelegate.IsBound());
	MapNameStr = GGetMapNameDelegate.Execute();
#endif		// WITH_ENGINE

	const FString PlatformStr(FPlatformProperties::PlatformName());

	/** This is meant to hold the name of the "sessions" that is occurring **/
	static bool bSetProfilingSessionFolderName = false;
	static FString ProfilingSessionFolderName = TEXT(""); 

	// here we want to have just the same profiling session name so all of the files will go into that folder over the course of the run so you don't just have a ton of folders
	FString FolderName;
	if( bSetProfilingSessionFolderName == false )
	{
		// now create the string
		FolderName = FString::Printf(TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *FDateTime::Now().ToString(TEXT("%m.%d-%H.%M.%S")));
		FolderName = FolderName.Right(MaxFilenameLen);

		ProfilingSessionFolderName = FolderName;
		bSetProfilingSessionFolderName = true;
	}
	else
	{
		FolderName = ProfilingSessionFolderName;
	}

	// now create the string
	// NOTE: due to the changelist this is implicitly using the same directory
	FString FolderNameOfProfileNoDate = FString::Printf( TEXT("%s-%s-%i"), *MapNameStr, *PlatformStr, GetChangeListNumberForPerfTesting() );
	FolderNameOfProfileNoDate = FolderNameOfProfileNoDate.Right(MaxFilenameLen);


	FString NameOfProfile = FString::Printf(TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *FDateTime::Now().ToString(TEXT("%d-%H.%M.%S")));
	NameOfProfile = NameOfProfile.Right(MaxFilenameLen);

	FString FileNameWithExtension = FString::Printf( TEXT("%s%s"), *NameOfProfile, *InFileExtension );
	FileNameWithExtension = FileNameWithExtension.Right(MaxFilenameLen);

	FString Filename;
	if( bIncludeDateForDirectoryName == true )
	{
		Filename = FolderName / FileNameWithExtension;
	}
	else
	{
		Filename = FolderNameOfProfileNoDate / FileNameWithExtension;
	}


	Retval = Filename;

	return Retval;
}



FString CreateProfileDirectoryAndFilename( const FString& InSubDirectoryName, const FString& InFileExtension )
{
	FString MapNameStr;
#if WITH_ENGINE
	checkf(GGetMapNameDelegate.IsBound());
	MapNameStr = GGetMapNameDelegate.Execute();
#endif		// WITH_ENGINE
	const FString PlatformStr = FString(TEXT("PC"));


	// create Profiling dir and sub dir
	const FString PathName = (FPaths::ProfilingDir() + InSubDirectoryName + TEXT("/"));
	IFileManager::Get().MakeDirectory( *PathName );
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *PathName );

	// create the directory name of this profile
	FString NameOfProfile = FString::Printf(TEXT("%s-%s-%s"), *MapNameStr, *PlatformStr, *FDateTime::Now().ToString(TEXT("%m.%d-%H.%M")));	
	NameOfProfile = NameOfProfile.Right(MaxFilenameLen);

	IFileManager::Get().MakeDirectory( *(PathName+NameOfProfile) );
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *(PathName+NameOfProfile) );


	// create the actual file name
	FString FileNameWithExtension = FString::Printf( TEXT("%s%s"), *NameOfProfile, *InFileExtension );
	FileNameWithExtension = FileNameWithExtension.Left(MaxFilenameLen);
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *FileNameWithExtension );


	FString Filename = PathName / NameOfProfile / FileNameWithExtension;
	//UE_LOG(LogProfilingDebugging, Warning, TEXT( "CreateProfileDirectoryAndFilename: %s"), *Filename );

	return Filename;
}
