// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LineDrawer.generated.h"

USTRUCT()
struct ADVANCEDLINEDRAWER_API FALDSplineTangentSettings
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
struct ADVANCEDLINEDRAWER_API FALDLineDescriptor
{
	GENERATED_BODY();

	int32 AddPoint(const FVector2D& Point, float InterpT, EInterpCurveMode InterpMode = CIM_CurveUser, const FVector2D& ArriveTangent = FVector2D::Zero(), const FVector2D& LeaveTangent = FVector2D::Zero());
	void SetPointsWithAutoTangents(const TArray<FVector2D>& Points, float InterpStartT = 0.0f, float InterpEndT = 1.0f, EInterpCurveMode InterpMode = CIM_CurveUser, const FALDSplineTangentSettings& TangentSettings = FALDSplineTangentSettings());

	UPROPERTY(EditAnywhere)
	float InterpCurveStartT = 0.0f;

	UPROPERTY(EditAnywhere)
	float InterpCurveEndT = 1.0f;

	UPROPERTY(EditAnywhere)
	float Thickness = 4.0f;

	UPROPERTY(EditAnywhere)
	float Resolution = 16;

	UPROPERTY(EditAnywhere)
	float DynamicResolutionFactor = 1.0f;

	UPROPERTY(EditAnywhere)
	float MaxResolution = 128;

	UPROPERTY(EditAnywhere)
	FSlateBrush Brush;

	FInterpCurve<FVector2D> InterpCurve;
};

class ADVANCEDLINEDRAWER_API ILineDrawer
{
public:
	virtual ~ILineDrawer() = default;

	int32 AddLine(const FALDLineDescriptor& LineDescriptor);
	bool UpdateLine(int32 LineIndex, TFunctionRef<bool(FALDLineDescriptor& OutLineDescriptor)> Updater);
	void RemoveLine(int32 LineIndex);
	void RemoveAllLines();

	const FALDLineDescriptor* GetLine(int32 LineIndex);

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

		uint32 InterpCurveDirtyHash = 0;
		uint32 VertexDataDirtyHash = 0;

		TArray<TTuple<float, FVector2D, FVector2D>> InterpCurveEvalResultCache;
	};

	struct FLineData
	{
		FALDLineDescriptor LineDescriptor;
		FRenderData RenderData;
	};
	mutable TSparseArray<FLineData> LineDatas;

protected:
	static void UpdateRenderData(const FALDLineDescriptor& LineDescriptor, FRenderData& InOutRenderData, const FGeometry& AllottedGeometry);
};
