// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivate.h"

#if UE_ENABLE_ICU
#include <unicode/regex.h>
#include "Regex.h"
#include "ICUUtilities.h"

namespace
{
	TSharedRef<icu::RegexPattern> CreateRegexPattern(const FString& SourceString)
	{
		icu::UnicodeString ICUSourceString;
		ICUUtilities::Convert(SourceString, ICUSourceString);

		UErrorCode ICUStatus = U_ZERO_ERROR;
		return MakeShareable( icu::RegexPattern::compile(ICUSourceString, 0, ICUStatus) );
	}
}

class FRegexPatternImplementation
{
public:
	FRegexPatternImplementation(const FString& SourceString) : ICURegexPattern( CreateRegexPattern(SourceString) ) 
	{
	}

public:
	TSharedRef<icu::RegexPattern> ICURegexPattern;
};

FRegexPattern::FRegexPattern(const FString& SourceString) : Implementation(new FRegexPatternImplementation(SourceString))
{
}

namespace
{
	TSharedRef<icu::RegexMatcher> CreateRegexMatcher(const FRegexPatternImplementation& Pattern, const icu::UnicodeString& InputString)
	{
		UErrorCode ICUStatus = U_ZERO_ERROR;
		return MakeShareable( Pattern.ICURegexPattern->matcher(InputString, ICUStatus) );
	}
}

class FRegexMatcherImplementation
{
public:
	FRegexMatcherImplementation(const FRegexPatternImplementation& Pattern, const FString& InputString) : ICUInputString( ICUUtilities::Convert(InputString) ), ICURegexMatcher( CreateRegexMatcher(Pattern, ICUInputString) )
	{
	}

public:
	const icu::UnicodeString ICUInputString;
	TSharedRef<icu::RegexMatcher> ICURegexMatcher;
};

FRegexMatcher::FRegexMatcher(const FRegexPattern& Pattern, const FString& InputString) : Implementation(new FRegexMatcherImplementation(Pattern.Implementation.Get(), InputString))
{
}	

bool FRegexMatcher::FindNext()
{
	return Implementation->ICURegexMatcher->find() != 0;
}

int32 FRegexMatcher::GetMatchBeginning()
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->start(ICUStatus);
}

int32 FRegexMatcher::GetMatchEnding()
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->end(ICUStatus);
}

int32 FRegexMatcher::GetCaptureGroupBeginning(const int32 Index)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->start(Index, ICUStatus);
}

int32 FRegexMatcher::GetCaptureGroupEnding(const int32 Index)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	return Implementation->ICURegexMatcher->end(Index, ICUStatus);
}

int32 FRegexMatcher::GetBeginLimit()
{
	return Implementation->ICURegexMatcher->regionStart();
}

int32 FRegexMatcher::GetEndLimit()
{
	return Implementation->ICURegexMatcher->regionEnd();
}

void FRegexMatcher::SetLimits(const int32 BeginIndex, const int32 EndIndex)
{
	UErrorCode ICUStatus = U_ZERO_ERROR;
	Implementation->ICURegexMatcher->region(BeginIndex, EndIndex, ICUStatus);
}
#endif