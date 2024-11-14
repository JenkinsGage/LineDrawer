﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

namespace AdvancedLineDrawer
{
	using FCurvePointType = FVector2d;

	struct FLineDescriptor
	{
		FCurvePointType StartPoint;
		FCurvePointType EndPoint;
		float InterpStartT = 0.0f;
		float InterpEndT = 1.0f;
		float Thickness = 8.0f;
		float Resolution = 32.0f;
		FSlateBrush Brush;
		FInterpCurve<FCurvePointType> InterpCurve;
	};

	struct FRenderData
	{
		TArray<FSlateVertex> VertexData;
		TArray<SlateIndex> IndexData;
		FSlateResourceHandle RenderingResourceHandle;
	};

	namespace LineBuilder
	{
		FInterpCurve<FCurvePointType> BuildCubicInterpCurveWithAutoTangents(const FCurvePointType& Start, const FCurvePointType& End);
		void UpdateRenderData(const FLineDescriptor& LineDescriptor, FRenderData& OutRenderData);
	}
}

class ADVANCEDLINEDRAWER_API ILineDrawer
{
public:
	virtual ~ILineDrawer() = default;

	int32 AddLine(AdvancedLineDrawer::FLineDescriptor&& LineDescriptor, bool bAutoBuildInterpCurve = true);
	bool UpdateLine(int32 LineIndex, TFunctionRef<bool(AdvancedLineDrawer::FLineDescriptor& OutLineDescriptor)> Updater);
	void RemoveLine(int32 LineIndex);

protected:
	virtual SWidget& GetLineDrawerWidget() = 0;

	void AddLineDrawerReferencedObjects(FReferenceCollector& Collector);
	int32 DrawLines(FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	struct FLineData
	{
		AdvancedLineDrawer::FLineDescriptor LineDescriptor;
		AdvancedLineDrawer::FRenderData RenderData;
	};
	TSparseArray<FLineData> LineDatas;
};