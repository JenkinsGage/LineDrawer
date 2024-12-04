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
	FVector2f SplineTangentFromHorizontalDelta = {1.0f, 0.0f};

	UPROPERTY(EditAnywhere)
	FVector2f SplineTangentFromVerticalDelta = {1.0f, 0.0f};
};

USTRUCT()
struct ADVANCEDLINEDRAWER_API FLineDescriptor
{
	GENERATED_BODY();

	void SetCurvePointsWithAutoTangents(const TArray<FVector2f>& Points, float InterpStartT = 0.0f, float InterpEndT = 1.0f, EInterpCurveMode InterpMode = CIM_CurveUser, const FSplineTangentSettings& TangentSettings = FSplineTangentSettings());

	int32 AddPoint(const FVector2f& Point, float InterpT, EInterpCurveMode InterpMode = CIM_CurveAuto, const FVector2f& ArriveTangent = FVector2f::Zero(), const FVector2f& LeaveTangent = FVector2f::Zero());

	FInterpCurve<FVector2f> InterpCurve;

	UPROPERTY(EditAnywhere)
	float Thickness = 4.0f;

	UPROPERTY(EditAnywhere)
	float Resolution = 4;

	UPROPERTY(EditAnywhere)
	float DynamicResolutionFactor = 1.0f;

	UPROPERTY(EditAnywhere)
	float MaxResolution = 64;

	UPROPERTY(EditAnywhere)
	float InterpCurveStartT = 0.0f;

	UPROPERTY(EditAnywhere)
	float InterpCurveEndT = 1.0f;

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

	TArray<int32> GetAllLines() const;
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
	};

	struct FLineData
	{
		FLineDescriptor LineDescriptor;
		bool bNeedReEvalInterpCurve = false;
		TArray<FVector2f> InterpCurveSamplePoints;

		FRenderData RenderData;
	};
	mutable TSparseArray<FLineData> LineDatas;

	static void UpdateLineRenderData(FLineData& InOutLineData, const FGeometry& AllottedGeometry);

	struct FLineBuilder
	{
		FLineBuilder(FRenderData& RenderData, const FSlateRenderTransform& RenderTransform, float ElementScale, float HalfThickness, float FilterRadius, float MiterAngleLimit);

		void BuildLineGeometry(const TArray<FVector2f>& Points, const FColor& PointColor, ESlateVertexRounding Rounding);
		void MakeStartCap(const FVector2f Position, const FVector2f Direction, float SegmentLength, const FVector2f Up, const FColor& Color, ESlateVertexRounding Rounding);
		void MakeEndCap(const FVector2f Position, const FVector2f Direction, float SegmentLength, const FVector2f Up, const FColor& Color, ESlateVertexRounding Rounding);

		static void AddQuadIndices(FRenderData& InRenderData);
		static FVector2f GetMiterNormal(const FVector2f InboundSegmentDir, const FVector2f OutboundSegmentDir);

		FRenderData& RenderData;
		const FSlateRenderTransform& RenderTransform;

		const float LocalHalfThickness;
		const float LocalFilterRadius;
		const float LocalCapLength;
		const float AngleCosineLimit;
		float PositionAlongLine = 0.f;
	};
};
