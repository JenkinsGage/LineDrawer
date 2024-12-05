// Fill out your copyright notice in the Description page of Project Settings.


#include "LineDrawer.h"

int32 GLineDrawerUpdateLineNumInParallel = 8;
FAutoConsoleVariableRef CVarLineDrawerUpdateLineNumInParallel(
	TEXT("r.LineDrawerUpdateLineNumInParallel"),
	GLineDrawerUpdateLineNumInParallel,
	TEXT("Min number of lines that will be distributed to each worker thread."),
	ECVF_Default
);

bool GLineDrawerForceSingleThread = false;
FAutoConsoleVariableRef CVarLineDrawerForceSingleThread(
	TEXT("r.LineDrawerForceSingleThread"),
	GLineDrawerForceSingleThread,
	TEXT("If true all the parallelisms of line drawer will be disabled.")
);

void FLineDescriptor::SetCurvePointsWithAutoTangents(const TArray<FVector2f>& Points, float InterpStartT, float InterpEndT, EInterpCurveMode InterpMode, const FSplineTangentSettings& TangentSettings)
{
	const int32 NumPoints = Points.Num();
	InterpCurve.Points.SetNumUninitialized(NumPoints);
	FVector2f NextArriveTangent = FVector2f::Zero();
	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		const FVector2f& CurrentPoint = Points[Index];
		const float InterpT = FMath::Lerp(InterpStartT, InterpEndT, NumPoints > 1 ? static_cast<float>(Index) / (NumPoints - 1) : 1.0f);

		if (Index < NumPoints - 1)
		{
			const FVector2f DeltaPos = Points[Index + 1] - CurrentPoint;
			if (InterpMode == CIM_Linear)
			{
				InterpCurve.Points[Index] = FInterpCurvePoint(InterpT, CurrentPoint, NextArriveTangent, DeltaPos, InterpMode);
				NextArriveTangent = -DeltaPos;
				continue;
			}
			const float ClampedTensionX = FMath::Min<float>(FMath::Abs<float>(DeltaPos.X), TangentSettings.SplineHorizontalDeltaRange);
			const float ClampedTensionY = FMath::Min<float>(FMath::Abs<float>(DeltaPos.Y), TangentSettings.SplineVerticalDeltaRange);
			const FVector2f SplineTangent = (ClampedTensionX * TangentSettings.SplineTangentFromHorizontalDelta + ClampedTensionY * TangentSettings.SplineTangentFromVerticalDelta) * FVector2f(FMath::Sign(DeltaPos.X), FMath::Sign(DeltaPos.Y));

			InterpCurve.Points[Index] = FInterpCurvePoint(InterpT, CurrentPoint, NextArriveTangent, SplineTangent, InterpMode);
			NextArriveTangent = TangentSettings.bTranspose ? FVector2f(-SplineTangent.Y, DeltaPos.X > 0 ? SplineTangent.X : -SplineTangent.X) : FVector2f(SplineTangent.X, -SplineTangent.Y);
		}
		else
		{
			InterpCurve.Points[Index] = FInterpCurvePoint(InterpT, CurrentPoint, NextArriveTangent, FVector2f::Zero(), InterpMode);
		}
	}
}

int32 FLineDescriptor::AddPoint(const FVector2f& Point, float InterpT, EInterpCurveMode InterpMode, const FVector2f& ArriveTangent, const FVector2f& LeaveTangent)
{
	const int32 NewCurvePointIndex = InterpCurve.AddPoint(InterpT, Point);
	auto& NewCurvePoint = InterpCurve.Points[NewCurvePointIndex];

	NewCurvePoint.InterpMode = InterpMode;
	NewCurvePoint.ArriveTangent = ArriveTangent;
	NewCurvePoint.LeaveTangent = LeaveTangent;

	return NewCurvePointIndex;
}

int32 ILineDrawer::AddLine(const FLineDescriptor& LineDescriptor)
{
	FLineData NewLineData;
	NewLineData.LineDescriptor = LineDescriptor;
	NewLineData.bNeedReEvalInterpCurve = true;

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
		LineData.bNeedReEvalInterpCurve = true;
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

TArray<int32> ILineDrawer::GetAllLines() const
{
	TArray<int32> Indexes;
	Indexes.Reserve(LineDatas.Num());
	for (auto It = LineDatas.CreateConstIterator(); It; ++It)
	{
		Indexes.Add(It.GetIndex());
	}

	return Indexes;
}

const FLineDescriptor* ILineDrawer::GetLine(int32 LineIndex)
{
	if (!LineDatas.IsValidIndex(LineIndex))
	{
		return nullptr;
	}

	return &LineDatas[LineIndex].LineDescriptor;
}

UMaterialInstanceDynamic* ILineDrawer::GetOrCreateMaterialInstanceOfLine(int32 LineIndex)
{
	if (!LineDatas.IsValidIndex(LineIndex))
	{
		return nullptr;
	}

	FLineData& LineData = LineDatas[LineIndex];
	UObject* ResourceObject = LineData.LineDescriptor.Brush.GetResourceObject();
	if (!ResourceObject)
	{
		return nullptr;
	}

	UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject);
	if (!Material)
	{
		return nullptr;
	}

	if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(Material))
	{
		return ExistingMID;
	}

	UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(Material, nullptr);
	LineData.LineDescriptor.Brush.SetResourceObject(NewMID);
	LineData.RenderData.RenderingResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(LineData.LineDescriptor.Brush);
	return NewMID;
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
	TRACE_CPUPROFILER_EVENT_SCOPE(ILineDrawer::DrawLines);

	ParallelFor(TEXT("ILineDrawer::ParallelUpdateLineRenderData"), LineDatas.Num(), GLineDrawerUpdateLineNumInParallel, [this, &AllottedGeometry](int32 Index)
	{
		UpdateLineRenderData(LineDatas[Index], AllottedGeometry);
	}, GLineDrawerForceSingleThread ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None);

	for (FLineData& LineData : LineDatas)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ILineDrawer::DrawLines::DrawElements);
		FRenderData& RenderData = LineData.RenderData;
		if (RenderData.VertexData.Num() == 0 || RenderData.IndexData.Num() == 0)
		{
			continue;
		}

		if(!RenderData.RenderingResourceHandle.IsValid())
		{
			RenderData.RenderingResourceHandle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(LineData.LineDescriptor.Brush);
		}

		FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, RenderData.RenderingResourceHandle, RenderData.VertexData, RenderData.IndexData, nullptr, 0, 0);
	}

	return LayerId;
}

void ILineDrawer::UpdateLineRenderData(FLineData& InOutLineData, const FGeometry& AllottedGeometry)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ILineDrawer::UpdateLineRenderData);

	auto& LineDescriptor = InOutLineData.LineDescriptor;
	if (InOutLineData.bNeedReEvalInterpCurve)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ILineDrawer::UpdateLineRenderData::EvalInterpCurve);
		InOutLineData.InterpCurveSamplePoints.Reset();
		auto& KeyPoints = LineDescriptor.InterpCurve.Points;
		if (KeyPoints.Num() > 0)
		{
			const float DynamicResolutionScale = LineDescriptor.DynamicResolutionFactor * AllottedGeometry.GetLocalSize().Length();
			constexpr float DynamicResolutionUnitCube = 512.0f * 512.0f * 512.0f;
			check(DynamicResolutionScale >= 0);
			check(LineDescriptor.Resolution > 0);
			TArray<float, TInlineAllocator<64>> EvalTValues;

			float EvalT = LineDescriptor.InterpCurveStartT;
			int32 SmallestKeyPointIndex;
			for (SmallestKeyPointIndex = 0; SmallestKeyPointIndex < KeyPoints.Num() && KeyPoints[SmallestKeyPointIndex].InVal < EvalT; ++SmallestKeyPointIndex){}

			while (EvalT <= LineDescriptor.InterpCurveEndT)
			{
				if (KeyPoints.IsValidIndex(SmallestKeyPointIndex))
				{
					auto& KeyPoint = KeyPoints[SmallestKeyPointIndex];
					const float KeyT = KeyPoint.InVal;
					if (EvalT == KeyT)
					{
						++SmallestKeyPointIndex;
						if (KeyPoint.InterpMode == CIM_Linear)
						{
							EvalTValues.Add(EvalT);
							if (KeyPoints.IsValidIndex(SmallestKeyPointIndex))
							{
								EvalT = KeyPoints[SmallestKeyPointIndex].InVal;
							}
							else if (EvalT == LineDescriptor.InterpCurveEndT)
							{
								break;
							}

							continue;
						}
					}
					else if (EvalT > KeyT)
					{
						EvalT = KeyPoints[SmallestKeyPointIndex].InVal;
						++SmallestKeyPointIndex;
					}
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

			float LineLength = 0.0f;
			FVector2f PrevPoint;
			InOutLineData.InterpCurveSamplePoints.SetNumUninitialized(EvalTValues.Num());
			for (int32 Index = 0; Index < EvalTValues.Num(); ++Index)
			{
				const FVector2f CurvePoint = LineDescriptor.InterpCurve.Eval(EvalTValues[Index]);
				InOutLineData.InterpCurveSamplePoints[Index] = CurvePoint;
				if (Index > 0)
				{
					LineLength += (CurvePoint - PrevPoint).Size();
				}
				PrevPoint = CurvePoint;
			}
			InOutLineData.LineLength = LineLength;
		}

		InOutLineData.bNeedReEvalInterpCurve = false;
	}

	{
		auto& RenderData = InOutLineData.RenderData;
		RenderData.VertexData.Reset();
		RenderData.IndexData.Reset();
		const int32 NumSamples = InOutLineData.InterpCurveSamplePoints.Num();
		if (NumSamples < 2 || InOutLineData.LineLength <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
		constexpr float AntiAliasingFilterRadius = 2.0f;
		constexpr float MiterAngleLimit = 90.0f - KINDA_SMALL_NUMBER;
		FLineBuilder LineBuilder(RenderData, PaintGeometry.GetAccumulatedRenderTransform(), PaintGeometry.DrawScale, LineDescriptor.Thickness, AntiAliasingFilterRadius, MiterAngleLimit);
		FColor TintColor = LineDescriptor.Brush.TintColor.GetSpecifiedColor().ToFColor(true);
		LineBuilder.BuildLineGeometry(InOutLineData.InterpCurveSamplePoints, InOutLineData.LineLength, TintColor, ESlateVertexRounding::Enabled);
	}
}

ILineDrawer::FLineBuilder::FLineBuilder(FRenderData& RenderData, const FSlateRenderTransform& RenderTransform, float ElementScale, float HalfThickness, float FilterRadius, float MiterAngleLimit) :
	RenderData(RenderData),
	RenderTransform(RenderTransform),
	LocalHalfThickness((HalfThickness + FilterRadius) / ElementScale),
	LocalFilterRadius(FilterRadius / ElementScale),
	LocalCapLength((FilterRadius / ElementScale) * 2.0f),
	AngleCosineLimit(FMath::DegreesToRadians((180.0f - MiterAngleLimit) * 0.5f))
{
}

void ILineDrawer::FLineBuilder::BuildLineGeometry(const TArray<FVector2f>& Points, float InLineLength, const FColor& PointColor, ESlateVertexRounding Rounding)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ILineDrawer::FLineBuilder::BuildLineGeometry);

	check(Points.Num() >= 2);
	check(InLineLength > KINDA_SMALL_NUMBER);

	LineLength = InLineLength;
	FVector2f Position = Points[0];
	FVector2f NextPosition = Points[1];

	FVector2f SegDirection;
	float SegLength;

	(NextPosition - Position).ToDirectionAndLength(SegDirection, SegLength);
	FVector2f Up = SegDirection.GetRotated(90.0f) * LocalHalfThickness;
	PositionAlongLine = 0.0f;

	MakeStartCap(Position, SegDirection, SegLength, Up, PointColor, Rounding);

	PositionAlongLine += SegLength;

	const int32 LastPointIndex = Points.Num() - 1;
	for (int32 Point = 1; Point < LastPointIndex; ++Point)
	{
		const FVector2f LastDirection = SegDirection;
		const FVector2f LastUp = Up;
		const float LastLength = SegLength;

		Position = NextPosition;
		NextPosition = Points[Point + 1];

		(NextPosition - Position).ToDirectionAndLength(SegDirection, SegLength);
		Up = SegDirection.GetRotated(90.0f) * LocalHalfThickness;

		const FVector2f MiterNormal = GetMiterNormal(LastDirection, SegDirection);
		const float DistanceToMiterLine = FVector2f::DotProduct(Up, MiterNormal);

		const float DirDotMiterNormal = FVector2f::DotProduct(SegDirection, MiterNormal);
		const float MinSegmentLength = FMath::Min(LastLength, SegLength);

		if (MinSegmentLength > SMALL_NUMBER && DirDotMiterNormal >= AngleCosineLimit && (MinSegmentLength * 0.5f * DirDotMiterNormal) >= FMath::Abs(DistanceToMiterLine))
		{
			const float ParallelDistance = DistanceToMiterLine / DirDotMiterNormal;
			const FVector2f MiterUp = Up - (SegDirection * ParallelDistance);

			const float MiterOffset = FVector2f::DotProduct(SegDirection, MiterUp);
			RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + MiterUp),
			                                                 FVector2f((PositionAlongLine - MiterOffset) / LineLength, 1.0f),
			                                                 FVector2f(PositionAlongLine - MiterOffset, 1.0f), PointColor, {}, Rounding));
			RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position - MiterUp),
			                                                 FVector2f((PositionAlongLine + MiterOffset) / LineLength, 0.0f),
			                                                 FVector2f(PositionAlongLine + MiterOffset, 0.0f), PointColor, {}, Rounding));
			AddQuadIndices(RenderData);
		}
		else
		{
			MakeEndCap(Position, LastDirection, LastLength, LastUp, PointColor, Rounding);
			MakeStartCap(Position, SegDirection, SegLength, Up, PointColor, Rounding);
		}

		PositionAlongLine += SegLength;
	}

	MakeEndCap(NextPosition, SegDirection, SegLength, Up, PointColor, Rounding);
}

void ILineDrawer::FLineBuilder::MakeStartCap(const FVector2f Position, const FVector2f Direction, float SegmentLength, const FVector2f Up, const FColor& Color, ESlateVertexRounding Rounding)
{
	if (SegmentLength > SMALL_NUMBER)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ILineDrawer::FLineBuilder::MakeStartCap);

		const float InwardDistance = FMath::Min(LocalFilterRadius, SegmentLength * 0.5f);
		const float OutwardDistance = (InwardDistance - LocalCapLength);
		const FVector2f CapInward = Direction * InwardDistance;
		const FVector2f CapOutward = Direction * OutwardDistance;

		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapOutward + Up),
		                                                 FVector2f((PositionAlongLine + OutwardDistance) / LineLength, 1.0f),
		                                                 FVector2f(PositionAlongLine + OutwardDistance, 1.0f), Color, {}, Rounding));
		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapOutward - Up),
		                                                 FVector2f((PositionAlongLine + OutwardDistance) / LineLength, 0.0f),
		                                                 FVector2f(PositionAlongLine + OutwardDistance, 0.0f), Color, {}, Rounding));
		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapInward + Up),
		                                                 FVector2f((PositionAlongLine + InwardDistance) / LineLength, 1.0f),
		                                                 FVector2f(PositionAlongLine + InwardDistance, 1.0f), Color, {}, Rounding));
		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapInward - Up),
		                                                 FVector2f((PositionAlongLine + InwardDistance) / LineLength, 0.0f),
		                                                 FVector2f(PositionAlongLine + InwardDistance, 0.0f), Color, {}, Rounding));
		AddQuadIndices(RenderData);
	}
}

void ILineDrawer::FLineBuilder::MakeEndCap(const FVector2f Position, const FVector2f Direction, float SegmentLength, const FVector2f Up, const FColor& Color, ESlateVertexRounding Rounding)
{
	if (SegmentLength > SMALL_NUMBER)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ILineDrawer::FLineBuilder::MakeEndCap);

		const float InwardDistance = FMath::Min(LocalFilterRadius, SegmentLength * 0.5f);
		const float OutwardDistance = (InwardDistance - LocalCapLength);
		const FVector2f CapInward = Direction * -InwardDistance;
		const FVector2f CapOutward = Direction * OutwardDistance;

		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapInward + Up),
		                                                 FVector2f((PositionAlongLine - InwardDistance) / LineLength, 1.0f),
		                                                 FVector2f(PositionAlongLine - InwardDistance, 1.0f), Color, {}, Rounding));
		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapInward - Up),
		                                                 FVector2f((PositionAlongLine - InwardDistance) / LineLength, 0.0f),
		                                                 FVector2f(PositionAlongLine - InwardDistance, 0.0f), Color, {}, Rounding));
		AddQuadIndices(RenderData);

		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapOutward + Up),
		                                                 FVector2f((PositionAlongLine + OutwardDistance) / LineLength, 1.0f),
		                                                 FVector2f(PositionAlongLine + OutwardDistance, 1.0f), Color, {}, Rounding));
		RenderData.VertexData.Emplace(FSlateVertex::Make(RenderTransform, FVector2f(Position + CapOutward - Up),
		                                                 FVector2f((PositionAlongLine + OutwardDistance) / LineLength, 0.0f),
		                                                 FVector2f(PositionAlongLine + OutwardDistance, 0.0f), Color, {}, Rounding));
		AddQuadIndices(RenderData);
	}
}

void ILineDrawer::FLineBuilder::AddQuadIndices(FRenderData& InRenderData)
{
	const int32 NumVerts = InRenderData.VertexData.Num();

	auto IndexQuad = [](FRenderData& InRenderData, int32 TopLeft, int32 TopRight, int32 BottomRight, int32 BottomLeft)
	{
		InRenderData.IndexData.Emplace(TopLeft);
		InRenderData.IndexData.Emplace(TopRight);
		InRenderData.IndexData.Emplace(BottomRight);

		InRenderData.IndexData.Emplace(BottomRight);
		InRenderData.IndexData.Emplace(BottomLeft);
		InRenderData.IndexData.Emplace(TopLeft);
	};

	IndexQuad(InRenderData, NumVerts - 3, NumVerts - 1, NumVerts - 2, NumVerts - 4);
}

FVector2f ILineDrawer::FLineBuilder::GetMiterNormal(const FVector2f InboundSegmentDir, const FVector2f OutboundSegmentDir)
{
	const FVector2f DirSum = InboundSegmentDir + OutboundSegmentDir;
	const float SizeSquared = DirSum.SizeSquared();

	if (SizeSquared > SMALL_NUMBER)
	{
		return DirSum * FMath::InvSqrt(SizeSquared);
	}

	return InboundSegmentDir.GetRotated(90.0f);
}
