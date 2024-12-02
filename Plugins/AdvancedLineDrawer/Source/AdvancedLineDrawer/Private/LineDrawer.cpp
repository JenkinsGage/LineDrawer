// Fill out your copyright notice in the Description page of Project Settings.


#include "LineDrawer.h"

int32 GLineDrawerParallelismMinBatchSize = 64;
FAutoConsoleVariableRef CVarLineDrawerParallelismMinBatchSize(
	TEXT("r.LineDrawerParallelismMinBatchSize"),
	GLineDrawerParallelismMinBatchSize,
	TEXT("Min batch size per thread of line drawer parallelism."),
	ECVF_Default
);

int32 FLineDescriptor::AddPoint(const FVector2D& Point, float InterpT, EInterpCurveMode InterpMode, const FVector2D& ArriveTangent, const FVector2D& LeaveTangent)
{
	const int32 NewCurvePointIndex = InterpCurve.AddPoint(InterpT, Point);
	auto& NewCurvePoint = InterpCurve.Points[NewCurvePointIndex];

	NewCurvePoint.InterpMode = InterpMode;
	NewCurvePoint.ArriveTangent = ArriveTangent;
	NewCurvePoint.LeaveTangent = LeaveTangent;

	return NewCurvePointIndex;
}

void FLineDescriptor::SetPointsWithAutoTangents(const TArray<FVector2D>& Points, float InterpStartT, float InterpEndT, EInterpCurveMode InterpMode, const FSplineTangentSettings& TangentSettings)
{
	const int32 NumPoints = Points.Num();
	InterpCurve.Points.SetNumUninitialized(NumPoints);
	FVector2D NextArriveTangent = FVector2D::Zero();
	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		const FVector2D& CurrentPoint = Points[Index];
		const float InterpT = FMath::Lerp(InterpStartT, InterpEndT, NumPoints > 1 ? static_cast<float>(Index) / (NumPoints - 1) : 1.0f);

		if (Index < NumPoints - 1)
		{
			const FVector2D DeltaPos = Points[Index + 1] - CurrentPoint;
			const float ClampedTensionX = FMath::Min<float>(FMath::Abs<float>(DeltaPos.X), TangentSettings.SplineHorizontalDeltaRange);
			const float ClampedTensionY = FMath::Min<float>(FMath::Abs<float>(DeltaPos.Y), TangentSettings.SplineVerticalDeltaRange);
			const FVector2D SplineTangent = (ClampedTensionX * TangentSettings.SplineTangentFromHorizontalDelta + ClampedTensionY * TangentSettings.SplineTangentFromVerticalDelta) * FVector2D(FMath::Sign(DeltaPos.X), FMath::Sign(DeltaPos.Y));

			InterpCurve.Points[Index] = FInterpCurvePoint(InterpT, CurrentPoint, NextArriveTangent, SplineTangent, InterpMode);
			NextArriveTangent = TangentSettings.bTranspose ? FVector2D(-SplineTangent.Y, DeltaPos.X > 0 ? SplineTangent.X : -SplineTangent.X) : FVector2D(SplineTangent.X, -SplineTangent.Y);
		}
		else
		{
			InterpCurve.Points[Index] = FInterpCurvePoint(InterpT, CurrentPoint, NextArriveTangent, FVector2D::Zero(), InterpMode);
		}
	}
}

int32 ILineDrawer::AddLine(const FLineDescriptor& LineDescriptor)
{
	FLineData NewLineData;
	NewLineData.LineDescriptor = LineDescriptor;
	NewLineData.RenderData.bNeedReEvalInterpCurve = true;

	GetLineDrawerWidget().Invalidate(EInvalidateWidgetReason::Paint);
	return LineDatas.Emplace(MoveTemp(NewLineData));
}

bool ILineDrawer::UpdateLine(int32 LineIndex, TFunctionRef<bool(FLineDescriptor& OutLineDescriptor)> Updater)
{
	if (!LineDatas.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineData& LineData = LineDatas[LineIndex];
	if (Updater(LineData.LineDescriptor))
	{
		LineData.RenderData.bNeedReEvalInterpCurve = true;
		GetLineDrawerWidget().Invalidate(EInvalidateWidgetReason::Paint);
	}

	return true;
}

void ILineDrawer::RemoveLine(int32 LineIndex)
{
	if (LineDatas.IsValidIndex(LineIndex))
	{
		LineDatas.RemoveAt(LineIndex);
		GetLineDrawerWidget().Invalidate(EInvalidateWidgetReason::Paint);
	}
}

void ILineDrawer::RemoveAllLines()
{
	LineDatas.Empty();
	GetLineDrawerWidget().Invalidate(EInvalidateWidgetReason::Paint);
}

const FLineDescriptor* ILineDrawer::GetLine(int32 LineIndex)
{
	if (!LineDatas.IsValidIndex(LineIndex))
	{
		return nullptr;
	}

	return &LineDatas[LineIndex].LineDescriptor;
}

void ILineDrawer::AddLineDrawerReferencedObjects(FReferenceCollector& Collector) const
{
	for (FLineData& LineData : LineDatas)
	{
		LineData.LineDescriptor.Brush.AddReferencedObjects(Collector);
	}
}

DECLARE_STATS_GROUP(TEXT("LineDrawer"), STATGROUP_LineDrawer, STATCAT_Advanced);
int32 ILineDrawer::DrawLines(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("DrawLines"), STAT_LineDrawer_DrawLines, STATGROUP_LineDrawer);

	for (FLineData& LineData : LineDatas)
	{
		FRenderData& RenderData = LineData.RenderData;
		UpdateRenderData(LineData.LineDescriptor, RenderData, AllottedGeometry);
		if (RenderData.VertexData.Num() == 0 || RenderData.IndexData.Num() == 0)
		{
			continue;
		}

		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, RenderData.RenderingResourceHandle, RenderData.VertexData, RenderData.IndexData, nullptr, 0, 0);
	}

	return LayerId;
}

void ILineDrawer::UpdateRenderData(const FLineDescriptor& LineDescriptor, FRenderData& InOutRenderData, const FGeometry& AllottedGeometry)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UpdateRenderData"), STAT_LineDrawer_UpdateRenderData, STATGROUP_LineDrawer);

	if (InOutRenderData.bNeedReEvalInterpCurve)
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("EvalInterpCurve"), STAT_LineDrawer_EvalInterpCurve, STATGROUP_LineDrawer);

		InOutRenderData.InterpCurveEvalResultCache.Reset();
		auto& KeyPoints = LineDescriptor.InterpCurve.Points;
		if (KeyPoints.Num() > 0)
		{
			const float DynamicResolutionScale = LineDescriptor.DynamicResolutionFactor * AllottedGeometry.GetLocalSize().Length();
			constexpr float DynamicResolutionUnitCube = 512.0f * 512.0f * 512.0f;
			check(DynamicResolutionScale >= 0);
			check(LineDescriptor.Resolution > 0);
			TArray<float, TInlineAllocator<256>> EvalTValues;

			float EvalT = LineDescriptor.InterpCurveStartT;
			int32 SmallestKeyPointIndex;
			for (SmallestKeyPointIndex = 0; SmallestKeyPointIndex < KeyPoints.Num() && KeyPoints[SmallestKeyPointIndex].InVal < EvalT; ++SmallestKeyPointIndex){}

			while (EvalT <= LineDescriptor.InterpCurveEndT)
			{
				if (KeyPoints.IsValidIndex(SmallestKeyPointIndex) && EvalT > KeyPoints[SmallestKeyPointIndex].InVal)
				{
					EvalT = KeyPoints[SmallestKeyPointIndex].InVal;
					++SmallestKeyPointIndex;
				}

				EvalTValues.Add(EvalT);
				if (EvalT == LineDescriptor.InterpCurveEndT)
				{
					break;
				}

				const float SecondDerivativeSq = LineDescriptor.InterpCurve.EvalSecondDerivative(EvalT).SquaredLength();
				const float DynamicResolution = DynamicResolutionScale * SecondDerivativeSq / DynamicResolutionUnitCube;
				const float Resolution = FMath::Min(LineDescriptor.Resolution + DynamicResolution, LineDescriptor.MaxResolution);
				EvalT = FMath::Min(EvalT + 1.0f / Resolution, LineDescriptor.InterpCurveEndT);
			}

			InOutRenderData.InterpCurveEvalResultCache.SetNumUninitialized(EvalTValues.Num());
			ParallelFor(TEXT("LineDrawer EvalInterpCurve"), EvalTValues.Num(), GLineDrawerParallelismMinBatchSize, [&EvalTValues, &LineDescriptor, &InOutRenderData](int32 Index)
			{
				const float T = EvalTValues[Index];
				const FVector2D CurvePoint = LineDescriptor.InterpCurve.Eval(T);
				const FVector2D Tangent = LineDescriptor.InterpCurve.EvalDerivative(T).GetSafeNormal();
				const FVector2D ScaledNormal = Tangent.GetRotated(90.0f) * LineDescriptor.Thickness;
				InOutRenderData.InterpCurveEvalResultCache[Index] = MakeTuple(T, CurvePoint, ScaledNormal);
			});
		}

		InOutRenderData.bNeedReEvalInterpCurve = false;
	}

	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("UpdateVertexData"), STAT_LineDrawer_UpdateVertexData, STATGROUP_LineDrawer);

		const int32 NumSamples = InOutRenderData.InterpCurveEvalResultCache.Num();
		if (NumSamples <= 1)
		{
			InOutRenderData.VertexData.Empty();
			InOutRenderData.IndexData.Empty();
			return;
		}
		float InterpLength = LineDescriptor.InterpCurveEndT - LineDescriptor.InterpCurveStartT;
		if (InterpLength <= 0)
		{
			return;
		}

		InOutRenderData.VertexData.SetNumUninitialized(2 * NumSamples);
		InOutRenderData.IndexData.SetNumUninitialized(6 * (NumSamples - 1));

		const FColor TintColor = LineDescriptor.Brush.TintColor.GetSpecifiedColor().ToFColor(true);

		ParallelFor(TEXT("LineDrawer UpdateVertexData"), NumSamples, GLineDrawerParallelismMinBatchSize,[&InOutRenderData, &TintColor, &AllottedGeometry, &LineDescriptor, InterpLength](int32 Index)
		{
			const TTuple<float, FVector2D, FVector2D>& Sample = InOutRenderData.InterpCurveEvalResultCache[Index];
			const FVector2D VertexPos1 = AllottedGeometry.LocalToAbsolute(Sample.Get<1>() + Sample.Get<2>());
			const FVector2D VertexPos2 = AllottedGeometry.LocalToAbsolute(Sample.Get<1>() - Sample.Get<2>());

			const float EvalT = Sample.Get<0>();
			const float CoordinateU = (EvalT - LineDescriptor.InterpCurveStartT) / InterpLength;
			//TODO: UV Tiling
			{
				FSlateVertex& Vert1 = InOutRenderData.VertexData[2 * Index];
				Vert1.Position[0] = VertexPos1[0];
				Vert1.Position[1] = VertexPos1[1];
				Vert1.Color = TintColor;
				Vert1.MaterialTexCoords[0] = CoordinateU;
				Vert1.MaterialTexCoords[1] = 0.0f;
				Vert1.TexCoords[0] = CoordinateU;
				Vert1.TexCoords[1] = 0.0f;
				Vert1.TexCoords[2] = CoordinateU;
				Vert1.TexCoords[3] = 0.0f;
			}

			const int32 SecondVertIndex = 2 * Index + 1;
			{
				FSlateVertex& Vert2 = InOutRenderData.VertexData[SecondVertIndex];
				Vert2.Position[0] = VertexPos2[0];
				Vert2.Position[1] = VertexPos2[1];
				Vert2.Color = TintColor;
				Vert2.MaterialTexCoords[0] = CoordinateU;
				Vert2.MaterialTexCoords[1] = 1.0f;
				Vert2.TexCoords[0] = CoordinateU;
				Vert2.TexCoords[1] = 1.0f;
				Vert2.TexCoords[2] = CoordinateU;
				Vert2.TexCoords[3] = 1.0f;
			}

			if (Index > 0)
			{
				const int32 IndexDataStartIndex = 6 * (Index - 1);
				InOutRenderData.IndexData[IndexDataStartIndex] = SecondVertIndex - 2;
				InOutRenderData.IndexData[IndexDataStartIndex + 1] = SecondVertIndex - 1;
				InOutRenderData.IndexData[IndexDataStartIndex + 2] = SecondVertIndex;

				InOutRenderData.IndexData[IndexDataStartIndex + 3] = SecondVertIndex - 3;
				InOutRenderData.IndexData[IndexDataStartIndex + 4] = SecondVertIndex - 2;
				InOutRenderData.IndexData[IndexDataStartIndex + 5] = SecondVertIndex - 1;
			}
		});
	}

	if(!InOutRenderData.RenderingResourceHandle.IsValid())
	{
		InOutRenderData.RenderingResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(LineDescriptor.Brush);
	}
}
