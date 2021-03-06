// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef __PListNodeFile_h__
#define __PlistNodeFile_h__

#include "PListNode.h"

/** A Node representing the plist as a whole */
class FPListNodeFile : public IPListNode
{
public:
	FPListNodeFile(SPListEditorPanel* InEditorWidget)
		: IPListNode(InEditorWidget)
	{}

public:

	/** Validation check */
	virtual bool IsValid() OVERRIDE;

	/** Returns the array of children */
	virtual TArray<TSharedPtr<IPListNode> >& GetChildren() OVERRIDE;

	/** Adds a child to the internal array of the class */
	virtual void AddChild(TSharedPtr<IPListNode> InChild) OVERRIDE;

	/** Gets the type of the node via enum */
	virtual EPLNTypes GetType() OVERRIDE;

	/** Determines whether the node needs to generate widgets for columns, or just use the whole row without columns */
	virtual bool UsesColumns() OVERRIDE;

	/** Generates a widget for a TableView row */
	virtual TSharedRef<ITableRow> GenerateWidget(const TSharedRef<STableViewBase>& OwnerTable) OVERRIDE;

	/** Generate a widget for the specified column name */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName, int32 Depth, ITableRow* RowPtr) OVERRIDE;

	/** Gets an XML representation of the node's internals */
	virtual FString ToXML(const int32 indent = 0, bool bOutputKey = true) OVERRIDE;

	/** Refreshes anything necessary in the node, such as a text used in information displaying */
	virtual void Refresh() OVERRIDE;

	/** Calculate how many key/value pairs exist for this node. */
	virtual int32 GetNumPairs() OVERRIDE;

	/** Called when the filter changes */
	virtual void OnFilterTextChanged(FString NewFilter) OVERRIDE;

	/** When parents are refreshed, they can set the index of their children for proper displaying */
	virtual void SetIndex(int32 NewIndex) OVERRIDE;

private:

	/** All children of the file (everything) */
	TArray<TSharedPtr<IPListNode> > Children;
	/** Widget for displaying text on this row */
	TSharedPtr<STextBlock> TextWidget;
};

#endif
