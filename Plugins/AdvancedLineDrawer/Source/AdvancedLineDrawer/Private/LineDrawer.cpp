// Fill out your copyright notice in the Description page of Project Settings.


#include "LineDrawer.h"

#include "SlateOptMacros.h"

FInterpCurve<AdvancedLineDrawer::FCurvePointType> AdvancedLineDrawer::LineBuilder::BuildCubicInterpCurveWithAutoTangents(const FCurvePointType& Start, const FCurvePointType& End)
{
	FInterpCurve<FCurvePointType> Curve;
	auto& P1 = Curve.Points[Curve.AddPoint(0.0f, Start)];
	P1.LeaveTangent = FCurvePointType(End.X, Start.Y);
	P1.ArriveTangent = P1.LeaveTangent;
	P1.InterpMode = CIM_CurveUser;
	auto& P2 = Curve.Points[Curve.AddPoint(1.0f, End)];
	P2.ArriveTangent = FCurvePointType(Start.X, End.Y);
	P2.LeaveTangent = P2.ArriveTangent;
	P2.InterpMode = CIM_CurveUser;

	Curve.AutoSetTangents(0.5f, false);
	return Curve;
}

void AdvancedLineDrawer::LineBuilder::UpdateRenderData(const FLineDescriptor& LineDescriptor, FRenderData& OutRenderData)
{
	SCOPED_NAMED_EVENT(LineBuilderUpdateRenderData, FColor::White);
	const float InterpLengthT = LineDescriptor.InterpEndT - LineDescriptor.InterpStartT;
	check(LineDescriptor.Resolution > 1.0f && InterpLengthT > 0);

	OutRenderData.VertexData.Reset();
	OutRenderData.IndexData.Reset();

	float EvalT = LineDescriptor.InterpStartT;
	while (EvalT < LineDescriptor.InterpEndT)
	{
		EvalT = FMath::Clamp(EvalT, 0.0f, 1.0f);

		const FCurvePointType CurvePoint = LineDescriptor.InterpCurve.Eval(EvalT);
		const FCurvePointType Tangent = LineDescriptor.InterpCurve.EvalDerivative(EvalT).GetSafeNormal();
		const FCurvePointType Normal = Tangent.GetRotated(90.0f);
		const FCurvePointType SecondDerivative = LineDescriptor.InterpCurve.EvalSecondDerivative(EvalT);

		const FCurvePointType VertexPos1 = CurvePoint + Normal * LineDescriptor.Thickness;
		const FCurvePointType VertexPos2 = CurvePoint - Normal * LineDescriptor.Thickness;

		const FColor TintColor = LineDescriptor.Brush.TintColor.GetSpecifiedColor().ToFColor(true);
		const float CoordinateU = (EvalT - LineDescriptor.InterpStartT) / InterpLengthT;
		{
			FSlateVertex& NewVert1 = OutRenderData.VertexData[OutRenderData.VertexData.AddUninitialized()];
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
			FSlateVertex& NewVert2 = OutRenderData.VertexData[OutRenderData.VertexData.AddUninitialized()];
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

		const int32 LastVertexIndex = OutRenderData.VertexData.Num() - 1;
		if (LastVertexIndex >= 3)
		{
			OutRenderData.IndexData.Emplace(LastVertexIndex - 2);
			OutRenderData.IndexData.Emplace(LastVertexIndex - 1);
			OutRenderData.IndexData.Emplace(LastVertexIndex);

			OutRenderData.IndexData.Emplace(LastVertexIndex - 3);
			OutRenderData.IndexData.Emplace(LastVertexIndex - 2);
			OutRenderData.IndexData.Emplace(LastVertexIndex - 1);
		}

		const float DynamicResolution = LineDescriptor.Resolution + SecondDerivative.Length() / LineDescriptor.Resolution;
		EvalT = FMath::Min(EvalT + 1.0f / DynamicResolution, LineDescriptor.InterpEndT);
	}

	if(!OutRenderData.RenderingResourceHandle.IsValid())
	{
		OutRenderData.RenderingResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(LineDescriptor.Brush);
	}
}

int32 ILineDrawer::AddLine(AdvancedLineDrawer::FLineDescriptor&& LineDescriptor, bool bAutoBuildInterpCurve)
{
	FLineData NewLineData;
	NewLineData.LineDescriptor = MoveTemp(LineDescriptor);

	if (bAutoBuildInterpCurve)
	{
		NewLineData.LineDescriptor.InterpCurve = AdvancedLineDrawer::LineBuilder::BuildCubicInterpCurveWithAutoTangents(LineDescriptor.StartPoint, LineDescriptor.EndPoint);
	}
	AdvancedLineDrawer::LineBuilder::UpdateRenderData(NewLineData.LineDescriptor, NewLineData.RenderData);
	GetLineDrawerWidget().Invalidate(EInvalidateWidgetReason::Paint);

	return LineDatas.Emplace(MoveTemp(NewLineData));
}

bool ILineDrawer::UpdateLine(int32 LineIndex, TFunctionRef<bool(AdvancedLineDrawer::FLineDescriptor& OutLineDescriptor)> Updater)
{
	if (!LineDatas.IsValidIndex(LineIndex))
	{
		return false;
	}

	FLineData& LineData = LineDatas[LineIndex];
	if (Updater(LineData.LineDescriptor))
	{
		AdvancedLineDrawer::LineBuilder::UpdateRenderData(LineData.LineDescriptor, LineData.RenderData);
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

void ILineDrawer::AddLineDrawerReferencedObjects(FReferenceCollector& Collector)
{
	for (FLineData& LineData : LineDatas)
	{
		LineData.LineDescriptor.Brush.AddReferencedObjects(Collector);
	}
}

int32 ILineDrawer::DrawLines(FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	for (const FLineData& LineData : LineDatas)
	{
		const AdvancedLineDrawer::FRenderData& RenderData = LineData.RenderData;
		if (!RenderData.RenderingResourceHandle.IsValid() || RenderData.VertexData.Num() == 0 || RenderData.IndexData.Num() == 0)
		{
			continue;
		}

		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, RenderData.RenderingResourceHandle, RenderData.VertexData, RenderData.IndexData, nullptr, 0, 0);
	}

	return LayerId;
}
