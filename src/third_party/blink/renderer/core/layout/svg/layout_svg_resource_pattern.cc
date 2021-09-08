/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright 2014 The Chromium Authors. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_pattern.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/svg_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_fit_to_view_box.h"
#include "third_party/blink/renderer/core/svg/svg_pattern_element.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

struct PatternData {
  USING_FAST_MALLOC(PatternData);

 public:
  scoped_refptr<Pattern> pattern;
  AffineTransform transform;
};

LayoutSVGResourcePattern::LayoutSVGResourcePattern(SVGPatternElement* node)
    : LayoutSVGResourcePaintServer(node),
      should_collect_pattern_attributes_(true),
      attributes_wrapper_(MakeGarbageCollected<PatternAttributesWrapper>()),
      pattern_map_(MakeGarbageCollected<PatternMap>()) {}

void LayoutSVGResourcePattern::RemoveAllClientsFromCache() {
  NOT_DESTROYED();
  pattern_map_->clear();
  should_collect_pattern_attributes_ = true;
  To<SVGPatternElement>(*GetElement()).InvalidateDependentPatterns();
  MarkAllClientsForInvalidation(kPaintInvalidation);
}

void LayoutSVGResourcePattern::WillBeDestroyed() {
  NOT_DESTROYED();
  To<SVGPatternElement>(*GetElement()).InvalidateDependentPatterns();
  LayoutSVGResourcePaintServer::WillBeDestroyed();
}

void LayoutSVGResourcePattern::StyleDidChange(StyleDifference diff,
                                              const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGResourcePaintServer::StyleDidChange(diff, old_style);
  if (old_style)
    return;
  // The resource has been attached, any linked <pattern> may need to
  // re-evaluate its attributes.
  To<SVGPatternElement>(*GetElement()).InvalidateDependentPatterns();
}

bool LayoutSVGResourcePattern::RemoveClientFromCache(
    SVGResourceClient& client) {
  NOT_DESTROYED();
  auto entry = pattern_map_->find(&client);
  if (entry == pattern_map_->end())
    return false;
  pattern_map_->erase(entry);
  return true;
}

const PatternAttributes& LayoutSVGResourcePattern::EnsureAttributes() const {
  DCHECK(GetElement());
  // Validate pattern DOM state before building the actual pattern. This should
  // avoid tearing down the pattern we're currently working on. Preferably the
  // state validation should have no side-effects though.
  if (should_collect_pattern_attributes_) {
    attributes_wrapper_->Set(PatternAttributes());
    auto* pattern_element = To<SVGPatternElement>(GetElement());
    pattern_element->CollectPatternAttributes(
        attributes_wrapper_->Attributes());
    should_collect_pattern_attributes_ = false;
  }
  return Attributes();
}

bool LayoutSVGResourcePattern::FindCycleFromSelf() const {
  NOT_DESTROYED();
  const PatternAttributes& attributes = EnsureAttributes();
  const SVGPatternElement* content_element = attributes.PatternContentElement();
  if (!content_element)
    return false;
  const LayoutObject* content_object = content_element->GetLayoutObject();
  DCHECK(content_object);
  return FindCycleInDescendants(*content_object);
}

std::unique_ptr<PatternData> LayoutSVGResourcePattern::BuildPatternData(
    const FloatRect& object_bounding_box) {
  NOT_DESTROYED();
  auto pattern_data = std::make_unique<PatternData>();

  const PatternAttributes& attributes = EnsureAttributes();
  // If there's no content disable rendering of the pattern.
  if (!attributes.PatternContentElement())
    return pattern_data;

  // Spec: When the geometry of the applicable element has no width or height
  // and objectBoundingBox is specified, then the given effect (e.g. a gradient
  // or a filter) will be ignored.
  if (attributes.PatternUnits() ==
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox &&
      object_bounding_box.IsEmpty())
    return pattern_data;

  // Compute tile metrics.
  FloatRect tile_bounds = SVGLengthContext::ResolveRectangle(
      GetElement(), attributes.PatternUnits(), object_bounding_box,
      *attributes.X(), *attributes.Y(), *attributes.Width(),
      *attributes.Height());
  if (tile_bounds.IsEmpty())
    return pattern_data;

  AffineTransform tile_transform;
  if (attributes.HasViewBox()) {
    // An empty viewBox disables rendering of the pattern.
    if (attributes.ViewBox().IsEmpty())
      return pattern_data;
    tile_transform = SVGFitToViewBox::ViewBoxToViewTransform(
        attributes.ViewBox(), attributes.PreserveAspectRatio(),
        tile_bounds.Size());
  } else {
    // A viewBox overrides patternContentUnits, per spec.
    if (attributes.PatternContentUnits() ==
        SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
      tile_transform.Scale(object_bounding_box.Width(),
                           object_bounding_box.Height());
    }
  }

  pattern_data->pattern = Pattern::CreatePaintRecordPattern(
      AsPaintRecord(tile_bounds.Size(), tile_transform),
      FloatRect(FloatPoint(), tile_bounds.Size()));

  // Compute pattern space transformation.
  pattern_data->transform.Translate(tile_bounds.X(), tile_bounds.Y());
  pattern_data->transform.PreMultiply(attributes.PatternTransform());

  return pattern_data;
}

bool LayoutSVGResourcePattern::ApplyShader(
    const SVGResourceClient& client,
    const FloatRect& reference_box,
    const AffineTransform* additional_transform,
    PaintFlags& flags) {
  NOT_DESTROYED();
  ClearInvalidationMask();

  std::unique_ptr<PatternData>& pattern_data =
      pattern_map_->insert(&client, nullptr).stored_value->value;
  if (!pattern_data)
    pattern_data = BuildPatternData(reference_box);

  if (!pattern_data->pattern)
    return false;

  AffineTransform transform = pattern_data->transform;
  if (additional_transform)
    transform = *additional_transform * transform;
  pattern_data->pattern->ApplyToFlags(flags,
                                      AffineTransformToSkMatrix(transform));
  flags.setFilterQuality(kLow_SkFilterQuality);
  return true;
}

sk_sp<PaintRecord> LayoutSVGResourcePattern::AsPaintRecord(
    const FloatSize& size,
    const AffineTransform& tile_transform) const {
  NOT_DESTROYED();
  DCHECK(!should_collect_pattern_attributes_);

  AffineTransform content_transform;
  if (Attributes().PatternContentUnits() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox)
    content_transform = tile_transform;

  FloatRect bounds(FloatPoint(), size);
  const auto* pattern_layout_object = To<LayoutSVGResourceContainer>(
      Attributes().PatternContentElement()->GetLayoutObject());
  DCHECK(pattern_layout_object);
  DCHECK(!pattern_layout_object->NeedsLayout());

  SubtreeContentTransformScope content_transform_scope(content_transform);

  PaintRecordBuilder builder;
  for (LayoutObject* child = pattern_layout_object->FirstChild(); child;
       child = child->NextSibling())
    SVGObjectPainter(*child).PaintResourceSubtree(builder.Context());
  PaintRecorder paint_recorder;
  cc::PaintCanvas* canvas = paint_recorder.beginRecording(bounds);
  canvas->save();
  canvas->concat(AffineTransformToSkMatrix(tile_transform));
  builder.EndRecording(*canvas);
  canvas->restore();
  return paint_recorder.finishRecordingAsPicture();
}

}  // namespace blink
