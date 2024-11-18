// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LineDrawer.generated.h"

using FALDCurvePointType = FVector2d;

USTRUCT()
struct ADVANCEDLINEDRAWER_API FALDLineDescriptor
{
	GENERATED_BODY();

	int32 AddPoint(const FALDCurvePointType& Point, float InterpT, EInterpCurveMode InterpMode, const TOptional<FALDCurvePointType>& ArriveTangent, const TOptional<FALDCurvePointType>& LeaveTangent);
	void SetPointsWithAutoTangents(const TArray<FALDCurvePointType>& Points, float InterpStartT = 0.0f, float InterpEndT = 1.0f, EInterpCurveMode InterpMode = CIM_Linear);

	UPROPERTY(EditAnywhere)
	float CurveInterpStartT = 0.0f;

	UPROPERTY(EditAnywhere)
	float CurveInterpEndT = 1.0f;

	UPROPERTY(EditAnywhere)
	float Thickness = 8.0f;

	UPROPERTY(EditAnywhere)
	float Resolution = 16.0f;

	UPROPERTY(EditAnywhere)
	float DynamicResolutionFactor = 32.0f;

	UPROPERTY(EditAnywhere)
	float MaxResolution = 128.0f;

	UPROPERTY(EditAnywhere)
	FSlateBrush Brush;

	FInterpCurve<FALDCurvePointType> InterpCurve;
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
		uint32 DirtyHash = 0;
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
