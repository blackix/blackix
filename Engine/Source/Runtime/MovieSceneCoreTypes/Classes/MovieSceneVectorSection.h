// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneVectorSection.generated.h"

/**
 * A vector section
 */
UCLASS( DependsOn=UMovieSceneSection, MinimalAPI )
class UMovieSceneVectorSection : public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()
public:
	
	/**
	 * Updates this section
	 *
	 * @param Position	The position in time within the movie scene
	 */
	FVector4 Eval( float Position ) const;

	/** MovieSceneSection interface */
	virtual void MoveSection( float DeltaPosition ) OVERRIDE;
	virtual void DilateSection( float DilationFactor, float Origin ) OVERRIDE;
	
	/** 
	 * Adds a key to the section
	 *
	 * @param Time	The location in time where the key should be added
	 * @param Value	The value of the key
	 */
	void AddKey( float Time, const FVector4& Value );
	
	/** 
	 * Determines if a new key would be new data, or just a duplicate of existing data
	 *
	 * @param Time	The location in time where the key would be added
	 * @param Value	The value of the new key
	 * @return True if the new key would be new data, false if duplicate
	 */
	bool NewKeyIsNewData(float Time, const FVector4& Value) const;

	/** Gets one of four curves in this section */
	FRichCurve& GetCurve(const int32& Index) { return Curves[Index]; }

	/** Sets how many channels are to be used */
	void SetChannelsUsed(int32 InChannelsUsed) {ChannelsUsed = InChannelsUsed;}
	
	/** Gets the number of channels in use */
	int32 GetChannelsUsed() const {return ChannelsUsed;}

private:
	/** Vector t */
	UPROPERTY()
	FRichCurve Curves[4];

	/** How many curves are actually used */
	UPROPERTY()
	int32 ChannelsUsed;
};
