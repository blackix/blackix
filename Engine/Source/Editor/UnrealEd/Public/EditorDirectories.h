// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once 

/** The different directory identifiers */
namespace ELastDirectory
{
	enum Type
	{
		UNR,
		BRUSH,
		FBX,
		FBX_ANIM,
		GENERIC_IMPORT,
		GENERIC_EXPORT,
		GENERIC_OPEN,
		GENERIC_SAVE,
		MESH_IMPORT_EXPORT,
		WORLD_ROOT,
		LEVEL,
		PROJECT,
		MAX
	};
};

class UNREALED_API FEditorDirectories
{
public:
	static FEditorDirectories& Get();

	/** Initializes the "LastDir" array with default directories for loading/saving files */
	void LoadLastDirectories();

	/** Writes the current "LastDir" array back out to the config files */
	void SaveLastDirectories();

	/**
	 *	Fetches the last directory used for the specified type
	 *
	 *	@param	InLastDir	the identifier for the directory type
	 *	@return	FString		the string that was last set
	 */
	FString GetLastDirectory( ELastDirectory::Type InLastDir ) const;

	
	/**
	 *	Sets the last directory used for the specified type
	 *
	 *	@param	InLastDir	the identifier for the directory type
	 *	@param	InLastStr	the string to set it as
	 */
	void SetLastDirectory( ELastDirectory::Type InLastDir, const FString& InLastStr );


private:
	/** Array of the last directories used for various editor windows */
	FString LastDir[ELastDirectory::MAX];
};
