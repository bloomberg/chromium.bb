// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGFilterPainter.h"

#include "core/layout/svg/LayoutSVGResourceFilter.h"
#include "core/paint/FilterEffectBuilder.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/svg/SVGFilterElement.h"
#include "core/svg/graphics/filters/SVGFilterBuilder.h"
#include "platform/graphics/filters/Filter.h"
#include "platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "platform/graphics/filters/SourceGraphic.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

GraphicsContext* SVGFilterRecordingContext::BeginContent(
    FilterData* filter_data) {
  DCHECK_EQ(filter_data->state_, FilterData::kInitial);

  // Create a new context so the contents of the filter can be drawn and cached.
  paint_controller_ = PaintController::Create();
  context_ = WTF::WrapUnique(new GraphicsContext(*paint_controller_));

  // Content painted into a new PaintRecord in SPv2 will have an
  // independent property tree set.
  if (RuntimeEnabledFeatures::slimmingPaintV2Enabled()) {
    paint_controller_->UpdateCurrentPaintChunkProperties(
        nullptr, PropertyTreeState::Root());
  }

  filter_data->state_ = FilterData::kRecordingContent;
  return context_.get();
}

void SVGFilterRecordingContext::EndContent(FilterData* filter_data) {
  DCHECK_EQ(filter_data->state_, FilterData::kRecordingContent);

  Filter* filter = filter_data->last_effect->GetFilter();
  SourceGraphic* source_graphic = filter->GetSourceGraphic();
  DCHECK(source_graphic);

  // Use the context that contains the filtered content.
  DCHECK(paint_controller_);
  DCHECK(context_);
  context_->BeginRecording(filter->FilterRegion());
  paint_controller_->CommitNewDisplayItems();

  paint_controller_->GetPaintArtifact().Replay(filter->FilterRegion(),
                                               *context_);

  SkiaImageFilterBuilder::BuildSourceGraphic(source_graphic,
                                             context_->EndRecording());

  // Content is cached by the source graphic so temporaries can be freed.
  paint_controller_ = nullptr;
  context_ = nullptr;

  filter_data->state_ = FilterData::kReadyToPaint;
}

static void PaintFilteredContent(GraphicsContext& context,
                                 const LayoutObject& object,
                                 FilterData* filter_data) {
  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          context, object, DisplayItem::kSVGFilter))
    return;

  FloatRect filter_bounds =
      filter_data ? filter_data->last_effect->GetFilter()->FilterRegion()
                  : FloatRect();
  LayoutObjectDrawingRecorder recorder(context, object, DisplayItem::kSVGFilter,
                                       filter_bounds);
  if (!filter_data || filter_data->state_ != FilterData::kReadyToPaint)
    return;
  DCHECK(filter_data->last_effect->GetFilter()->GetSourceGraphic());

  filter_data->state_ = FilterData::kPaintingFilter;

  FilterEffect* last_effect = filter_data->last_effect;
  sk_sp<SkImageFilter> image_filter =
      SkiaImageFilterBuilder::Build(last_effect, kColorSpaceDeviceRGB);
  context.Save();

  // Clip drawing of filtered image to the minimum required paint rect.
  context.ClipRect(last_effect->MapRect(object.StrokeBoundingBox()));

  context.BeginLayer(1, SkBlendMode::kSrcOver, &filter_bounds, kColorFilterNone,
                     std::move(image_filter));
  context.EndLayer();
  context.Restore();

  filter_data->state_ = FilterData::kReadyToPaint;
}

GraphicsContext* SVGFilterPainter::PrepareEffect(
    const LayoutObject& object,
    SVGFilterRecordingContext& recording_context) {
  filter_.ClearInvalidationMask();

  if (FilterData* filter_data = filter_.GetFilterDataForLayoutObject(&object)) {
    // If the filterData already exists we do not need to record the content
    // to be filtered. This can occur if the content was previously recorded
    // or we are in a cycle.
    if (filter_data->state_ == FilterData::kPaintingFilter)
      filter_data->state_ = FilterData::kPaintingFilterCycleDetected;

    if (filter_data->state_ == FilterData::kRecordingContent)
      filter_data->state_ = FilterData::kRecordingContentCycleDetected;

    return nullptr;
  }

  SVGFilterGraphNodeMap* node_map = SVGFilterGraphNodeMap::Create();
  FilterEffectBuilder builder(nullptr, object.ObjectBoundingBox(), 1);
  Filter* filter = builder.BuildReferenceFilter(
      toSVGFilterElement(*filter_.GetElement()), nullptr, node_map);
  if (!filter || !filter->LastEffect())
    return nullptr;

  IntRect source_region = EnclosingIntRect(
      Intersection(filter->FilterRegion(), object.StrokeBoundingBox()));
  filter->GetSourceGraphic()->SetSourceRect(source_region);

  FilterData* filter_data = FilterData::Create();
  filter_data->last_effect = filter->LastEffect();
  filter_data->node_map = node_map;

  // TODO(pdr): Can this be moved out of painter?
  filter_.SetFilterDataForLayoutObject(const_cast<LayoutObject*>(&object),
                                       filter_data);
  return recording_context.BeginContent(filter_data);
}

void SVGFilterPainter::FinishEffect(
    const LayoutObject& object,
    SVGFilterRecordingContext& recording_context) {
  FilterData* filter_data = filter_.GetFilterDataForLayoutObject(&object);
  if (filter_data) {
    // A painting cycle can occur when an FeImage references a source that
    // makes use of the FeImage itself. This is the first place we would hit
    // the cycle so we reset the state and continue.
    if (filter_data->state_ == FilterData::kPaintingFilterCycleDetected)
      filter_data->state_ = FilterData::kPaintingFilter;

    // Check for RecordingContent here because we may can be re-painting
    // without re-recording the contents to be filtered.
    if (filter_data->state_ == FilterData::kRecordingContent)
      recording_context.EndContent(filter_data);

    if (filter_data->state_ == FilterData::kRecordingContentCycleDetected)
      filter_data->state_ = FilterData::kRecordingContent;
  }
  PaintFilteredContent(recording_context.PaintingContext(), object,
                       filter_data);
}

}  // namespace blink
