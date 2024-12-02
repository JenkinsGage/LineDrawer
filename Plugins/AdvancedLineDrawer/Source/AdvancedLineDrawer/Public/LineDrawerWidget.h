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

USTRUCT()
struct FLineWithAutoTangent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<FVector2D> Points;

	UPROPERTY(EditAnywhere)
	float InterpStartT = 0.0f;

	UPROPERTY(EditAnywhere)
	float InterpEndT = 1.0f;

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EInterpCurveMode> InterpMode = CIM_Linear;

	UPROPERTY(EditAnywhere)
	FLineDescriptor LineDescriptor;

	int32 LineIndex = INDEX_NONE;

	void WritePointsToLineDescriptor(const FSplineTangentSettings& TangentSettings = FSplineTangentSettings());
};

/**
 * Example widget to draw line.
 */
UCLASS()
class ULineDrawerWidget : public UWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	UPROPERTY(EditAnywhere)
	TArray<FLineWithAutoTangent> Lines;

	UPROPERTY(EditAnywhere)
	FSplineTangentSettings AutoTangentSettings;

	TSharedPtr<SLineDrawerWidget> MyLineDrawerWidget;
};
