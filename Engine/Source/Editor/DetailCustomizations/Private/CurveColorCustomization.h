// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class UObject;
class SCurveEditor;
class IPropertyHandle;
class FDetailWidgetRow;
class IDetailChildrenBuilder;

struct FRuntimeCurveLinearColor;

/**
 * Customizes a RuntimeCurveLinearColor struct to display a Curve Editor
 */
class FCurveColorCustomization : public IPropertyTypeCustomization, public FCurveOwnerInterface
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/**
	 * Destructor
	 */
	virtual ~FCurveColorCustomization();

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader( TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;
	virtual void CustomizeChildren( TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils ) override;

	/** FCurveOwnerInterface interface */
	virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
	virtual TArray<FRichCurveEditInfo> GetCurves() override;
	virtual UObject* GetOwner() override;
	virtual void ModifyOwner() override;
	virtual void MakeTransactional() override;
	virtual void OnCurveChanged() override;
	virtual bool IsLinearColorCurve() const override { return true; }
	virtual FLinearColor GetLinearColorValue(float InTime) const override;
	virtual bool HasAnyAlphaKeys() const override;
	virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo );

private:
	/**
	 * Constructor
	 */
	FCurveColorCustomization();

	/**
	 * Get View Min Input for the Curve Editor
	 */
	float GetViewMinInput() const { return ViewMinInput; }

	/**
	 * Get View Min Input for the Curve Editor
	 */
	float GetViewMaxInput() const { return ViewMaxInput; }

	/**
	 * Get Timeline Length for the Curve Editor
	 */
	float GetTimelineLength() const;

	/**
	 * Set View Min and Max Inputs for the Curve Editor
	 */
	void SetInputViewRange(float InViewMinInput, float InViewMaxInput);

	/**
	 * Called when RuntimeCurveLinearColor's External Curve is changed
	 */
	void OnExternalCurveChanged(TSharedRef<IPropertyHandle> CurvePropertyHandle);

	/**
	 * Called when button clicked to create an External Colour Curve
	 */
	FReply OnCreateButtonClicked();

	/**
	 * Whether Create button is enabled
	 */
	bool IsCreateButtonEnabled() const;

	/**
	 * Called when button clicked to convert External Curve to internal curve
	 */
	FReply OnConvertButtonClicked();

	/**
	 * Whether Convert button is enabled
	 */
	bool IsConvertButtonEnabled() const;

	/**
	 * Called when user double clicks on the curve preview to open a full size editor
	 */
	FReply OnCurvePreviewDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent );

	/**
	 * Copies data from one Rich Curve to another
	 */
	static void CopyCurveData( const FRichCurve* SrcCurve, FRichCurve* DestCurve );

	/**
	 * Destroys the pop out window used for editing internal curves.
	 */
	void DestroyPopOutWindow();

private:
	/** Cached RuntimeCurveLinearColor struct handle */
	TSharedPtr<IPropertyHandle> StructPropertyHandle;

	/** Cached External Curve handle */
	TSharedPtr<IPropertyHandle> ExternalCurveHandle;

	/** Small preview Curve Editor */
	TSharedPtr<SCurveEditor> CurveWidget;

	/** Window for pop out Curve Editor */
	TWeakPtr<SWindow> CurveEditorWindow;

	/** Cached pointer to the actual RuntimeCurveLinearColor struct */
	FRuntimeCurveLinearColor* RuntimeCurve;

	/** Object that owns the RuntimeFloatCurve */
	UObject* Owner;

	/** View Min Input for the Curve Editor */
	float ViewMinInput;

	/** View Max Input for the Curve Editor */
	float ViewMaxInput;

	/** Size of the pop out Curve Editor window */
	static const FVector2D DEFAULT_WINDOW_SIZE;
};