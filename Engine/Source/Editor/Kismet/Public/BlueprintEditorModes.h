// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BlueprintEditorModule.h"
#include "BlueprintUtilities.h"


struct KISMET_API FBlueprintEditorApplicationModes
{
	// Mode identifiers
	static const FName StandardBlueprintEditorMode;
	static const FName BlueprintDefaultsMode;
	static const FName BlueprintComponentsMode;
	static const FName BlueprintInterfaceMode;
	static const FName BlueprintMacroMode;
	static FText GetLocalizedMode( const FName InMode )
	{
		static TMap< FName, FText > LocModes;

		if (LocModes.Num() == 0)
		{
			LocModes.Add( StandardBlueprintEditorMode, NSLOCTEXT("BlueprintEditor", "StandardBlueprintEditorMode", "Graph") );
			LocModes.Add( BlueprintDefaultsMode, NSLOCTEXT("BlueprintEditor", "BlueprintDefaultsMode", "Defaults") );
			LocModes.Add( BlueprintComponentsMode, NSLOCTEXT("BlueprintEditor", "BlueprintComponentsMode", "Components") );
			LocModes.Add( BlueprintInterfaceMode, NSLOCTEXT("BlueprintEditor", "BlueprintInterfaceMode", "Interface") );
			LocModes.Add( BlueprintMacroMode, NSLOCTEXT("BlueprintEditor", "BlueprintMacroMode", "Macro") );
		}

		check( InMode != NAME_None );
		const FText* OutDesc = LocModes.Find( InMode );
		check( OutDesc );
		return *OutDesc;
	}
private:
	FBlueprintEditorApplicationModes() {}
};



class KISMET_API FBlueprintEditorApplicationMode : public FApplicationMode
{
public:
	FBlueprintEditorApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor, FName InModeName, const bool bRegisterViewport = true, const bool bRegisterDefaultsTab = true);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) OVERRIDE;
	virtual void PreDeactivateMode() OVERRIDE;
	virtual void PostActivateMode() OVERRIDE;
public:

protected:
	TWeakPtr<FBlueprintEditor> MyBlueprintEditor;

	// Set of spawnable tabs in blueprint editing mode
	FWorkflowAllowedTabSet BlueprintEditorTabFactories;

	// Set of spawnable tabs useful in derived classes, even without a blueprint
	FWorkflowAllowedTabSet CoreTabFactories;

	// Set of spawnable tabs only usable in blueprint editing mode (not useful in Persona, etc...)
	FWorkflowAllowedTabSet BlueprintEditorOnlyTabFactories;
};


class KISMET_API FBlueprintDefaultsApplicationMode : public FApplicationMode
{
public:
	FBlueprintDefaultsApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) OVERRIDE;
	virtual void PostActivateMode() OVERRIDE;
public:

protected:
	TWeakPtr<FBlueprintEditor> MyBlueprintEditor;

	// Set of spawnable tabs in blueprint defaults mode
	FWorkflowAllowedTabSet BlueprintDefaultsTabFactories;
};


class KISMET_API FBlueprintComponentsApplicationMode : public FApplicationMode
{
public:
	FBlueprintComponentsApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) OVERRIDE;
	virtual void PreDeactivateMode() OVERRIDE;
	virtual void PostActivateMode() OVERRIDE;

	TSharedPtr<FBlueprintEditor> GetBlueprintEditor() const { return MyBlueprintEditor.Pin(); }
	AActor* GetPreviewActor() const;
protected:
	TWeakPtr<FBlueprintEditor> MyBlueprintEditor;

	// Set of spawnable tabs in this mode
	FWorkflowAllowedTabSet BlueprintComponentsTabFactories;
};

class KISMET_API FBlueprintInterfaceApplicationMode : public FApplicationMode
{
public:
	FBlueprintInterfaceApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) OVERRIDE;
public:

protected:
	TWeakPtr<FBlueprintEditor> MyBlueprintEditor;

	// Set of spawnable tabs in this mode
	FWorkflowAllowedTabSet BlueprintInterfaceTabFactories;
};

class KISMET_API FBlueprintMacroApplicationMode : public FApplicationMode
{
public:
	FBlueprintMacroApplicationMode(TSharedPtr<class FBlueprintEditor> InBlueprintEditor);

	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) OVERRIDE;
public:

protected:
	TWeakPtr<FBlueprintEditor> MyBlueprintEditor;

	// Set of spawnable tabs in this mode
	FWorkflowAllowedTabSet BlueprintMacroTabFactories;
};