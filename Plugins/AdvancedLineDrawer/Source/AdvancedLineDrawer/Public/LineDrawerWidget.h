// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LineDrawer.h"
#include "Components/Widget.h"
#include "Widgets/SLeafWidget.h"
#include "LineDrawerWidget.generated.h"

/**
 * Example widget to use the ILineDrawer
 */
class SLineDrawerWidget : public SLeafWidget, public FGCObject, public ILineDrawer
{
public:
	SLATE_BEGIN_ARGS(SLineDrawerWidget)
	{}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	//~ Begin ILineDrawer Interface
	virtual SWidget& GetLineDrawerWidget() override { return *this; }
	//~ End ILineDrawer Interface

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;
	//~ End FGCObject Interface

	//~ Begin SLeafWidget Interface
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	//~ End SLeafWidget Interface
};

UCLASS()
class ADVANCEDLINEDRAWER_API ULineDrawerWidget : public UWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	int32 AddLine(const FVector2D& StartPoint, const FVector2D& EndPoint, const FSlateBrush& Brush, float Thickness = 8.0f, float InterpStartT = 0.0f, float InterpEndT = 1.0f, float Resolution = 32.0f);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	TSharedPtr<SLineDrawerWidget> MyLineDrawerWidget;
};
