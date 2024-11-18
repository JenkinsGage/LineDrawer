// Fill out your copyright notice in the Description page of Project Settings.


#include "LineDrawer.h"

int32 FALDLineDescriptor::AddPoint(const FALDCurvePointType& Point, float InterpT, EInterpCurveMode InterpMode, const FALDCurvePointType& ArriveTangent, const FALDCurvePointType& LeaveTangent)
{
	const int32 NewCurvePointIndex = InterpCurve.AddPoint(InterpT, Point);
	auto& NewCurvePoint = InterpCurve.Points[NewCurvePointIndex];

	NewCurvePoint.InterpMode = InterpMode;
	NewCurvePoint.ArriveTangent = ArriveTangent;
	NewCurvePoint.LeaveTangent = LeaveTangent;

	return NewCurvePointIndex;
}

void FALDLineDescriptor::SetPointsWithAutoTangents(const TArray<FALDCurvePointType>& Points, float InterpStartT, float InterpEndT, EInterpCurveMode InterpMode, const FALDSplineTangentSettings& TangentSettings)
{
	const int32 NumPoints = Points.Num();
	InterpCurve.Reset();
	InterpCurve.Points.AddUninitialized(NumPoints);
	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		const FALDCurvePointType& CurrentPoint = Points[Index];
		const float InterpT = FMath::Lerp(InterpStartT, InterpEndT, NumPoints > 1 ? static_cast<float>(Index) / (NumPoints - 1) : 1.0f);

		if (Index < NumPoints - 1)
		{
			const FALDCurvePointType SplineTangent = ComputeSplineTangent(CurrentPoint, Points[Index + 1], TangentSettings);
			const FALDCurvePointType LeaveTangent = TangentSettings.bTranspose ? FALDCurvePointType(-SplineTangent.Y, -SplineTangent.X) : -SplineTangent;
			InterpCurve.Points[Index] = FInterpCurvePoint(InterpT, CurrentPoint, SplineTangent, LeaveTangent, InterpMode);
		}
	}
}

FALDCurvePointType FALDLineDescriptor::ComputeSplineTangent(const FALDCurvePointType& Start, const FALDCurvePointType& End, const FALDSplineTangentSettings& Settings)
{
	const FVector2D DeltaPos = End - Start;
	const bool bGoingForward = DeltaPos.X >= 0.0f;

	const float ClampedTensionX = FMath::Min<float>(FMath::Abs<float>(DeltaPos.X), bGoingForward ? Settings.ForwardSplineHorizontalDeltaRange : Settings.BackwardSplineHorizontalDeltaRange);
	const float ClampedTensionY = FMath::Min<float>(FMath::Abs<float>(DeltaPos.Y), bGoingForward ? Settings.ForwardSplineVerticalDeltaRange : Settings.BackwardSplineVerticalDeltaRange);

	if (bGoingForward)
	{
		return (ClampedTensionX * Settings.ForwardSplineTangentFromHorizontalDelta) + (ClampedTensionY * Settings.ForwardSplineTangentFromVerticalDelta);
	}
	else
	{
		return (ClampedTensionX * Settings.BackwardSplineTangentFromHorizontalDelta) + (ClampedTensionY * Settings.BackwardSplineTangentFromVerticalDelta);
	}
}

int32 ILineDrawer::AddLine(const FALDLineDescriptor& LineDescriptor)
{
	FLineData NewLineData;
	NewLineData.LineDescriptor = LineDescriptor;

	GetLineDrawerWidget().Invalidate(EInvalidateWidgetReason::Paint);
	return LineDatas.Emplace(MoveTemp(NewLineData));
}

bool ILineDrawer::UpdateLine(int32 LineIndex, TFunctionRef<bool(FALDLineDescriptor& OutLineDescriptor)> Updater)
{
	if (!LineDatas.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineData& LineData = LineDatas[LineIndex];
	if (Updater(LineData.LineDescriptor))
	{
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

const FALDLineDescriptor* ILineDrawer::GetLine(int32 LineIndex)
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

int32 ILineDrawer::DrawLines(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
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

void ILineDrawer::UpdateRenderData(const FALDLineDescriptor& LineDescriptor, FRenderData& InOutRenderData, const FGeometry& AllottedGeometry)
{
	QUICK_SCOPE_CYCLE_COUNTER(ILineDrawer_UpdateRenderData);
	{
		uint32 GeometryHash = FCrc::TypeCrc32(AllottedGeometry);
		uint32 DescriptorHash = FCrc::TypeCrc32(LineDescriptor);
		uint32 DirtyHash = HashCombineFast(GeometryHash, DescriptorHash);
		if (DirtyHash == InOutRenderData.DirtyHash)
		{
			return;
		}
		InOutRenderData.DirtyHash = DirtyHash;
	}

	const float InterpLengthT = LineDescriptor.CurveInterpRange.Size<float>();
	check(LineDescriptor.Resolution > 1.0f && InterpLengthT > 0);
	const FColor TintColor = LineDescriptor.Brush.TintColor.GetSpecifiedColor().ToFColor(true);
	const float DynamicResolutionCorr = LineDescriptor.DynamicResolutionFactor / AllottedGeometry.GetLocalSize().SquaredLength();

	InOutRenderData.VertexData.Reset();
	InOutRenderData.IndexData.Reset();

	float EvalT = LineDescriptor.CurveInterpRange.GetLowerBoundValue();
	while (EvalT < LineDescriptor.CurveInterpRange.GetUpperBoundValue())
	{
		EvalT = FMath::Clamp(EvalT, 0.0f, 1.0f);

		const FALDCurvePointType CurvePoint = LineDescriptor.InterpCurve.Eval(EvalT);
		const FALDCurvePointType Tangent = LineDescriptor.InterpCurve.EvalDerivative(EvalT).GetSafeNormal();
		const FALDCurvePointType ScaledNormal = Tangent.GetRotated(90.0f) * LineDescriptor.Thickness;
		const FALDCurvePointType SecondDerivative = LineDescriptor.InterpCurve.EvalSecondDerivative(EvalT);

		const FALDCurvePointType VertexPos1 = AllottedGeometry.LocalToAbsolute(CurvePoint + ScaledNormal);
		const FALDCurvePointType VertexPos2 = AllottedGeometry.LocalToAbsolute(CurvePoint - ScaledNormal);

		const float CoordinateU = (EvalT - LineDescriptor.CurveInterpRange.GetLowerBoundValue()) / InterpLengthT;
		{
			FSlateVertex& NewVert1 = InOutRenderData.VertexData[InOutRenderData.VertexData.AddUninitialized()];
			NewVert1.Position[0] = VertexPos1[0];
			NewVert1.Position[1] = VertexPos1[1];
			NewVert1.Color = TintColor;
			//TODO: UV Tiling
			NewVert1.MaterialTexCoords[0] = CoordinateU;
			NewVert1.MaterialTexCoords[1] = 0.0f;
			NewVert1.TexCoords[0] = CoordinateU;
			NewVert1.TexCoords[1] = 0.0f;
			NewVert1.TexCoords[2] = CoordinateU;
			NewVert1.TexCoords[3] = 0.0f;
		}

		{
			FSlateVertex& NewVert2 = InOutRenderData.VertexData[InOutRenderData.VertexData.AddUninitialized()];
			NewVert2.Position[0] = VertexPos2[0];
			NewVert2.Position[1] = VertexPos2[1];
			NewVert2.Color = TintColor;
			//TODO: UV Tiling
			NewVert2.MaterialTexCoords[0] = CoordinateU;
			NewVert2.MaterialTexCoords[1] = 1.0f;
			NewVert2.TexCoords[0] = CoordinateU;
			NewVert2.TexCoords[1] = 1.0f;
			NewVert2.TexCoords[2] = CoordinateU;
			NewVert2.TexCoords[3] = 1.0f;
		}

		const int32 LastVertexIndex = InOutRenderData.VertexData.Num() - 1;
		if (LastVertexIndex >= 3)
		{
			InOutRenderData.IndexData.Emplace(LastVertexIndex - 2);
			InOutRenderData.IndexData.Emplace(LastVertexIndex - 1);
			InOutRenderData.IndexData.Emplace(LastVertexIndex);

			InOutRenderData.IndexData.Emplace(LastVertexIndex - 3);
			InOutRenderData.IndexData.Emplace(LastVertexIndex - 2);
			InOutRenderData.IndexData.Emplace(LastVertexIndex - 1);
		}

		const float DynamicResolution = SecondDerivative.SquaredLength() * DynamicResolutionCorr;
		const float Resolution = FMath::Min(LineDescriptor.Resolution + DynamicResolution, LineDescriptor.MaxResolution);
		EvalT = FMath::Min(EvalT + 1.0f / Resolution, LineDescriptor.CurveInterpRange.GetUpperBoundValue());
	}

	if(!InOutRenderData.RenderingResourceHandle.IsValid())
	{
		InOutRenderData.RenderingResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(LineDescriptor.Brush);
	}
}
