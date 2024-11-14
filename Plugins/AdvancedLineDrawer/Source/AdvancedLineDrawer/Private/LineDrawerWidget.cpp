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
	return DrawLines(OutDrawElements, LayerId);
}

FVector2D SLineDrawerWidget::ComputeDesiredSize(float) const
{
	return FVector2D(256.0f, 256.0f);
}

int32 ULineDrawerWidget::AddLine(const FVector2D& StartPoint, const FVector2D& EndPoint, const FSlateBrush& Brush, float Thickness, float InterpStartT, float InterpEndT, float Resolution)
{
	if (MyLineDrawerWidget.IsValid())
	{
		AdvancedLineDrawer::FLineDescriptor LineDescriptor;
		LineDescriptor.StartPoint = StartPoint;
		LineDescriptor.EndPoint = EndPoint;
		LineDescriptor.InterpStartT = InterpStartT;
		LineDescriptor.InterpEndT = InterpEndT;
		LineDescriptor.Thickness = Thickness;
		LineDescriptor.Resolution = Resolution;
		LineDescriptor.Brush = Brush;
		return MyLineDrawerWidget->AddLine(MoveTemp(LineDescriptor));
	}

	return INDEX_NONE;
}

TSharedRef<SWidget> ULineDrawerWidget::RebuildWidget()
{
	MyLineDrawerWidget = SNew(SLineDrawerWidget);
	return MyLineDrawerWidget.ToSharedRef();
}

void ULineDrawerWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyLineDrawerWidget.Reset();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
