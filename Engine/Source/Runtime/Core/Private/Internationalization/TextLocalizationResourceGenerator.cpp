// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "CorePrivate.h"
#include "InternationalizationManifest.h"
#include "InternationalizationManifestJsonSerializer.h"
#include "InternationalizationArchive.h"
#include "InternationalizationArchiveJsonSerializer.h"
#include "TextLocalizationResourceGenerator.h"

DEFINE_LOG_CATEGORY_STATIC(LogTextLocalizationResourceGenerator, Log, All);

TSharedPtr<FJsonObject> FTextLocalizationResourceGenerator::ReadJSONTextFile(const FString& InFilePath)
{
	//read in file as string
	FString FileContents;
	if ( !FFileHelper::LoadFileToString(FileContents, *InFilePath) )
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error,TEXT("Failed to load file %s."), *InFilePath);
		return NULL;
	}

	//parse as JSON
	TSharedPtr<FJsonObject> JSONObject;

	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FileContents );

	if( !FJsonSerializer::Deserialize( Reader, JSONObject ) || !JSONObject.IsValid())
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error,TEXT("Invalid JSON in file %s."), *InFilePath);
		return NULL;
	}

	return JSONObject;
}

void FTextLocalizationResourceGenerator::FLocalizationEntryTracker::ReportCollisions() const
{
	for(auto i = Namespaces.CreateConstIterator(); i; ++i)
	{
		const FString& NamespaceName = i.Key();
		const FLocalizationEntryTracker::FKeyTable& KeyTable = i.Value();
		for(auto j = KeyTable.CreateConstIterator(); j; ++j)
		{
			const FString& KeyName = j.Key();
			const FLocalizationEntryTracker::FEntryArray& EntryArray = j.Value();

			bool bWasCollisionDetected = false;
			for(int32 k = 0; k < EntryArray.Num(); ++k)
			{
				const FLocalizationEntryTracker::FEntry& LeftEntry = EntryArray[k];
				for(int32 l = k + 1; l < EntryArray.Num(); ++l)
				{
					const FLocalizationEntryTracker::FEntry& RightEntry = EntryArray[l];
					const bool bDoesLocalizedStringDiffer = !LeftEntry.LocalizedString.Equals( RightEntry.LocalizedString, ESearchCase::CaseSensitive );
					bWasCollisionDetected = bDoesLocalizedStringDiffer;
				}
			}

			if(bWasCollisionDetected)
			{
				FString CollidingEntryListString;
				for(int32 k = 0; k < EntryArray.Num(); ++k)
				{
					const FLocalizationEntryTracker::FEntry& Entry = EntryArray[k];

					if( !(CollidingEntryListString.IsEmpty()) )
					{
						CollidingEntryListString += '\n';
					}

					CollidingEntryListString += FString::Printf( TEXT("Archive: (%s) String: (%s)"), *(Entry.ArchiveName), *(Entry.LocalizedString) );
				}

				UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("Archives contain conflicting entries for (Namespace:%s, Key:%s):\n%s"), *NamespaceName, *KeyName, *CollidingEntryListString);
			}
		}
	}
}

bool FTextLocalizationResourceGenerator::FLocalizationEntryTracker::WriteToArchive(FArchive* const Archive)
{
	bool WasSuccessful = true;

	FLocalizationEntryTracker& NonConstThis = const_cast<FLocalizationEntryTracker&>(*this); // Necessary conversion because FArchive lacks const-correctness using the insertion operator.

	if ( Archive == NULL )
	{
		return false;
	}

	Archive->SetForceUnicode(true);

	// Write namespace count
	uint32 NamespaceCount = NonConstThis.Namespaces.Num();
	*Archive << NamespaceCount;

	// Iterate through namespaces
	for(auto NamespaceIterator = NonConstThis.Namespaces.CreateIterator(); NamespaceIterator; ++NamespaceIterator)
	{
		/*const*/ FString Namespace = NamespaceIterator.Key();
		/*const*/ FLocalizationEntryTracker::FKeyTable& KeyTable = NamespaceIterator.Value();

		// Write namespace.
		*Archive << Namespace;

		// Write key count.
		const int32 KeyCountOffsetInFile = Archive->Tell();
		const uint32 OriginalKeyCount = KeyTable.Num();
		uint32 KeyCount = OriginalKeyCount;
		*Archive << KeyCount;

		// Iterate through keys and values
		for(auto KeyIterator = KeyTable.CreateIterator(); KeyIterator; ++KeyIterator)
		{
			/*const*/ FString Key = KeyIterator.Key();
			/*const*/ FLocalizationEntryTracker::FEntryArray& EntryArray = KeyIterator.Value();

			// Skip this key if there are no entries.
			if(EntryArray.Num() == 0)
			{
				UE_LOG(LogTextLocalizationResourceGenerator, Warning, TEXT("Archives contained no entries for key (%s)"), *(Key));
				--KeyCount; // We've skipped an entry and thus a key and the key count must be adjusted.
				continue;
			}

			// Find first valid entry.
			/*const*/ FLocalizationEntryTracker::FEntry* Value = NULL;
			for(int32 i = 0; i < EntryArray.Num(); ++i)
			{
				if( !( EntryArray[i].LocalizedString.IsEmpty() ) )
				{
					Value = &(EntryArray[i]);
					break;
				}
			}

			// Skip this key if there is no valid entry.
			if( !(Value) )
			{
				UE_LOG(LogTextLocalizationResourceGenerator, Verbose, TEXT("Archives contained only blank entries for key (%s)"), *(Key));
				--KeyCount; // We've skipped an entry and thus a key and the key count must be adjusted.
				continue;
			}

			// Write key.
			*Archive << Key;

			// Write string entry.
			*Archive << Value->SourceStringHash;

			*Archive << Value->LocalizedString;
		}

		// If key count is different than original key count (due to skipped entries), adjust key count in file.
		if(KeyCount != OriginalKeyCount)
		{
			const int32 LatestOffsetInFile = Archive->Tell();
			Archive->Seek(KeyCountOffsetInFile);
			*Archive << KeyCount;
			Archive->Seek(LatestOffsetInFile);
		}
	}

	return WasSuccessful;
}

bool FTextLocalizationResourceGenerator::Generate(const FString& SourcePath, const TSharedRef<FInternationalizationManifest>& InternationalizationManifest, const FString& CultureToGenerate, FArchive* const DestinationArchive)
{
	FLocalizationEntryTracker LocalizationEntryTracker;

	FString CulturePath = SourcePath / *(CultureToGenerate);

	// Find archives in the culture-specific folder.
	TArray<FString> ArchiveFileNames;
	IFileManager::Get().FindFiles(ArchiveFileNames, *(CulturePath / TEXT("*.archive")), true, false);

	if(ArchiveFileNames.Num() == 0)
	{
		FString Message = FString::Printf(TEXT("No archives were found for culture %s."), *(CultureToGenerate));
		UE_LOG(LogTextLocalizationResourceGenerator, Warning, TEXT("%s"), *Message);
	}

	// For each archive:
	for(int32 ArchiveIndex = 0; ArchiveIndex < ArchiveFileNames.Num(); ++ArchiveIndex)
	{
		const FString ArchiveName = ArchiveFileNames[ArchiveIndex];

		// Read each archive file from the culture-named directory in the source path.
		FString ArchiveFilePath = CulturePath / ArchiveName;
		ArchiveFilePath = FPaths::ConvertRelativePathToFull(ArchiveFilePath);
		TSharedPtr<FJsonObject> ArchiveJSONObject = ReadJSONTextFile(ArchiveFilePath);
		if( !(ArchiveJSONObject.IsValid()) )
		{
			UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("No archive found at %s."), *ArchiveFilePath);
			continue;
		}
		TSharedRef<FInternationalizationArchive> InternationalizationArchive = MakeShareable( new FInternationalizationArchive );
		{
			FInternationalizationArchiveJsonSerializer ArchiveSerializer;
			ArchiveSerializer.DeserializeArchive(ArchiveJSONObject.ToSharedRef(), InternationalizationArchive);
		}

		// Generate text localization resource from manifest and archive entries.
		for(TManifestEntryByContextIdContainer::TConstIterator i = InternationalizationManifest->GetEntriesByContextIdIterator(); i; ++i)
		{
			// Gather relevant info from manifest entry.
			const TSharedRef<FManifestEntry>& ManifestEntry = i.Value();
			const FString& Namespace = ManifestEntry->Namespace;
			const FLocItem& Source = ManifestEntry->Source;
			const FString& SourceString = Source.Text;
			const FString UnescapedSourceString = SourceString.ReplaceEscapedCharWithChar();
			const uint32 SourceStringHash = FCrc::StrCrc32(*UnescapedSourceString);

			FLocalizationEntryTracker::FKeyTable& KeyTable = LocalizationEntryTracker.Namespaces.FindOrAdd(*Namespace);

			// Keeps track of the key strings of non-optional manifest entries that are missing a corresponding archive entry.
			FString MissingArchiveEntryKeyStrings;

			// Keeps track of the key strings of non-optional manifest entries that are missing a translation
			FString MissingArchiveTranslationKeyStrings;

			// Create a localization entry for each namespace and key combination.
			for(auto ContextIt = ManifestEntry->Contexts.CreateConstIterator(); ContextIt; ++ContextIt)
			{
				const FString& Key = ContextIt->Key;

				// Find matching archive entry.
				TSharedPtr<FArchiveEntry> ArchiveEntry = InternationalizationArchive->FindEntryBySource(Namespace, Source, ContextIt->KeyMetadataObj);

				if(ContextIt->bIsOptional && (!ArchiveEntry.IsValid() || (ArchiveEntry.IsValid() && ArchiveEntry->Translation.Text.IsEmpty())))
				{
					// Skip any optional manifest entries that do not have a matching archive entry or if the matching archive entry does not have a translation
					continue;
				}

				FString UnescapedTranslatedString;
				if( ArchiveEntry.IsValid() )
				{
					UnescapedTranslatedString = ArchiveEntry->Translation.Text.ReplaceEscapedCharWithChar();

					if(UnescapedTranslatedString.IsEmpty())
					{
						if ( !MissingArchiveTranslationKeyStrings.IsEmpty() )
						{
							MissingArchiveTranslationKeyStrings += TEXT(", ");
						}
						MissingArchiveTranslationKeyStrings += Key;
					}
				}
				else
				{
					if ( !MissingArchiveEntryKeyStrings.IsEmpty() )
					{
						MissingArchiveEntryKeyStrings += TEXT(", ");
					}
					MissingArchiveEntryKeyStrings += Key;
				}

				FLocalizationEntryTracker::FEntryArray& EntryArray = KeyTable.FindOrAdd(*Key);
				if(ArchiveEntry.IsValid())
				{
					FLocalizationEntryTracker::FEntry NewEntry;
					NewEntry.ArchiveName = ArchiveFilePath;
					NewEntry.LocalizedString = UnescapedTranslatedString;
					NewEntry.SourceStringHash = SourceStringHash;
					EntryArray.Add(NewEntry);
				}
			}

			if(!MissingArchiveEntryKeyStrings.IsEmpty())
			{
				FString KeyListString = FString::Printf(TEXT("[%s]"), *MissingArchiveEntryKeyStrings);
				FString Message = FString::Printf( TEXT("Archive (%s) contains no translation for entry (Namespace:%s, Source:%s) for keys: %s."), *ArchiveFilePath, *Namespace, *SourceString, *KeyListString);
				UE_LOG(LogTextLocalizationResourceGenerator, Verbose, TEXT("%s"), *Message);
			}

			if(!MissingArchiveTranslationKeyStrings.IsEmpty())
			{
				FString KeyListString = FString::Printf(TEXT("[%s]"), *MissingArchiveTranslationKeyStrings);
				FString Message = FString::Printf( TEXT("Archive (%s) contains empty translation for entry (Namespace:%s, Source:%s) with keys: %s."), *ArchiveFilePath, *Namespace, *SourceString, *KeyListString);
				UE_LOG(LogTextLocalizationResourceGenerator, Verbose, TEXT("%s"), *Message);
			}
		}
	}

	LocalizationEntryTracker.ReportCollisions();

	// Write resource.
	if( !(LocalizationEntryTracker.WriteToArchive(DestinationArchive)) )
	{
		UE_LOG(LogTextLocalizationResourceGenerator, Error, TEXT("Failed to write localization entries to archive."));
		return false;
	}

	return true;
}