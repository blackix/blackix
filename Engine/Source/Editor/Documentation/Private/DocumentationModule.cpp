// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "DocumentationModulePrivatePCH.h"
#include "Documentation.h"
#include "MultiBoxDefs.h"

class FDocumentationModule : public IDocumentationModule
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() OVERRIDE
	{
		Documentation = FDocumentation::Create();

		FMultiBoxSettings::ToolTipConstructor = FMultiBoxSettings::FConstructToolTip::CreateRaw( this, &FDocumentationModule::ConstructDefaultToolTip );
	}

	virtual void ShutdownModule() OVERRIDE
	{
		if ( FModuleManager::Get().IsModuleLoaded("Slate") )
		{
			FMultiBoxSettings::ResetToolTipConstructor();
		}

		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.
	}

	virtual class TSharedRef< IDocumentation > GetDocumentation() const OVERRIDE
	{
		return Documentation.ToSharedRef();
	}

private:

	TSharedRef< SToolTip > ConstructDefaultToolTip( const TAttribute<FText>& ToolTipText, const TSharedPtr<SWidget>& OverrideContent, const TSharedPtr<const FUICommandInfo>& Action )
	{
		if ( Action.IsValid() )
		{
			return Documentation->CreateToolTip( ToolTipText, OverrideContent, FString( TEXT("Shared/") ) + Action->GetBindingContext().ToString(), Action->GetCommandName().ToString() );
		}

		TSharedPtr< SWidget > ToolTipContent;
        if ( OverrideContent.IsValid() )
        {
            ToolTipContent = OverrideContent;
        }
        else
        {
            ToolTipContent = SNullWidget::NullWidget;
        }
        
		return SNew( SToolTip )
			   .Text( ToolTipText )
			   [
					ToolTipContent.ToSharedRef()
			   ];
	}

private:

	TSharedPtr< IDocumentation > Documentation;

};

IMPLEMENT_MODULE( FDocumentationModule, Documentation )