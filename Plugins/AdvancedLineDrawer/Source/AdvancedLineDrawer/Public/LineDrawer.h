// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LineDrawer.generated.h"

USTRUCT()
struct ADVANCEDLINEDRAWER_API FSplineTangentSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	bool bTranspose = true;

	UPROPERTY(EditAnywhere)
	float SplineHorizontalDeltaRange = 1000.0f;

	UPROPERTY(EditAnywhere)
	float SplineVerticalDeltaRange = 1000.0f;

	UPROPERTY(EditAnywhere)
	FVector2D SplineTangentFromHorizontalDelta = {1.0f, 0.0f};

	UPROPERTY(EditAnywhere)
	FVector2D SplineTangentFromVerticalDelta = {1.0f, 0.0f};
};

USTRUCT()
struct ADVANCEDLINEDRAWER_API FLineDescriptor
{
	GENERATED_BODY();

	void SetPointsWithAutoTangents(const TArray<FVector2D>& Points, float InterpStartT = 0.0f, float InterpEndT = 1.0f, EInterpCurveMode InterpMode = CIM_CurveUser, const FSplineTangentSettings& TangentSettings = FSplineTangentSettings());

	int32 AddPoint(const FVector2D& Point, float InterpT, EInterpCurveMode InterpMode = CIM_CurveUser, const FVector2D& ArriveTangent = FVector2D::Zero(), const FVector2D& LeaveTangent = FVector2D::Zero());

	FInterpCurve<FVector2D> InterpCurve;

	UPROPERTY(EditAnywhere)
	float InterpCurveStartT = 0.0f;

	UPROPERTY(EditAnywhere)
	float InterpCurveEndT = 1.0f;

	UPROPERTY(EditAnywhere)
	float Thickness = 4.0f;

	UPROPERTY(EditAnywhere)
	float Resolution = 4;

	UPROPERTY(EditAnywhere)
	float DynamicResolutionFactor = 1.0f;

	UPROPERTY(EditAnywhere)
	float MaxResolution = 64;

	UPROPERTY(EditAnywhere)
	FSlateBrush Brush;
};

class ADVANCEDLINEDRAWER_API ILineDrawer
{
public:
	virtual ~ILineDrawer() = default;

	int32 AddLine(const FLineDescriptor& LineDescriptor);
	bool UpdateLine(int32 LineIndex, TFunctionRef<bool(FLineDescriptor& OutLineDescriptor)> Updater);
	void RemoveLine(int32 LineIndex);
	void RemoveAllLines();

	const FLineDescriptor* GetLine(int32 LineIndex);
	UMaterialInstanceDynamic* GetOrCreateMaterialInstanceOfLine(int32 LineIndex);

protected:
	virtual SWidget& GetLineDrawerWidget() = 0;

	void AddLineDrawerReferencedObjects(FReferenceCollector& Collector) const;
	int32 DrawLines(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

private:
	struct FRenderData
	{
		TArray<FSlateVertex> VertexData;
		TArray<SlateIndex> IndexData;
		FSlateResourceHandle RenderingResourceHandle;

		bool bNeedReEvalInterpCurve = false;
		TArray<TTuple<float, FVector2D, FVector2D>> InterpCurveEvalResultCache;
	};

	struct FLineData
	{
		FLineDescriptor LineDescriptor;
		FRenderData RenderData;
	};
	mutable TSparseArray<FLineData> LineDatas;

protected:
	static void UpdateRenderData(const FLineDescriptor& LineDescriptor, FRenderData& InOutRenderData, const FGeometry& AllottedGeometry);
};
