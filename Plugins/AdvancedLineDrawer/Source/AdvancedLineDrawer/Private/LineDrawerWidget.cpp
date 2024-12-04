// Fill out your copyright notice in the Description page of Project Settings.


#include "LineDrawerWidget.h"

#include "SlateOptMacros.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SLineDrawerWidget::Construct(const FArguments& InArgs)
{
}

void SLineDrawerWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	AddLineDrawerReferencedObjects(Collector);
}

FString SLineDrawerWidget::GetReferencerName() const
{
	return TEXT("SLineDrawerWidget");
}

int32 SLineDrawerWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	return DrawLines(AllottedGeometry, OutDrawElements, LayerId);
}

FVector2D SLineDrawerWidget::ComputeDesiredSize(float) const
{
	return FVector2D(256.0f, 256.0f);
}

void FLineWithAutoTangent::WritePointsToLineDescriptor(const FSplineTangentSettings& TangentSettings)
{
	LineDescriptor.SetCurvePointsWithAutoTangents(Points, InterpStartT, InterpEndT, InterpMode, TangentSettings);
}

TSharedRef<SWidget> ULineDrawerWidget::RebuildWidget()
{
	MyLineDrawerWidget = SNew(SLineDrawerWidget);
	return MyLineDrawerWidget.ToSharedRef();
}

void ULineDrawerWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (MyLineDrawerWidget.IsValid())
	{
		for (FLineWithAutoTangent& Line : Lines)
		{
			Line.WritePointsToLineDescriptor(AutoTangentSettings);

			if (Line.LineIndex == INDEX_NONE)
			{
				Line.LineIndex = MyLineDrawerWidget->AddLine(Line.LineDescriptor);
			}
			else
			{
				if (MyLineDrawerWidget->GetLine(Line.LineIndex))
				{
					MyLineDrawerWidget->UpdateLine(Line.LineIndex, [&Line](FLineDescriptor& OutLineDescriptor)
					{
						OutLineDescriptor = Line.LineDescriptor;
						return true;
					});
				}
				else
				{
					Line.LineIndex = MyLineDrawerWidget->AddLine(Line.LineDescriptor);
				}
			}
		}
	}
}

void ULineDrawerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyLineDrawerWidget.Reset();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
