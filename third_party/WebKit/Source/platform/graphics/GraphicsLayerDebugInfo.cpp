/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "platform/graphics/GraphicsLayerDebugInfo.h"

#include "base/trace_event/trace_event_argument.h"
#include "platform/scroll/MainThreadScrollingReason.h"

namespace blink {

GraphicsLayerDebugInfo::GraphicsLayerDebugInfo()
    : compositing_reasons_(CompositingReason::kNone),
      squashing_disallowed_reasons_(SquashingDisallowedReason::kNone),
      owner_node_id_(0),
      main_thread_scrolling_reasons_(0) {}

GraphicsLayerDebugInfo::~GraphicsLayerDebugInfo() = default;

std::unique_ptr<base::trace_event::TracedValue>
GraphicsLayerDebugInfo::AsTracedValue() const {
  std::unique_ptr<base::trace_event::TracedValue> traced_value(
      new base::trace_event::TracedValue());
  AppendAnnotatedInvalidateRects(traced_value.get());
  AppendCompositingReasons(traced_value.get());
  AppendSquashingDisallowedReasons(traced_value.get());
  AppendOwnerNodeId(traced_value.get());
  AppendMainThreadScrollingReasons(traced_value.get());
  return traced_value;
}

void GraphicsLayerDebugInfo::AppendAnnotatedInvalidateRects(
    base::trace_event::TracedValue* traced_value) const {
  traced_value->BeginArray("annotated_invalidation_rects");
  for (const auto& annotated_rect : previous_invalidations_) {
    const FloatRect& rect = annotated_rect.rect;
    traced_value->BeginDictionary();
    traced_value->BeginArray("geometry_rect");
    traced_value->AppendDouble(rect.X());
    traced_value->AppendDouble(rect.Y());
    traced_value->AppendDouble(rect.Width());
    traced_value->AppendDouble(rect.Height());
    traced_value->EndArray();
    traced_value->SetString(
        "reason", PaintInvalidationReasonToString(annotated_rect.reason));
    traced_value->EndDictionary();
  }
  traced_value->EndArray();
}

void GraphicsLayerDebugInfo::AppendCompositingReasons(
    base::trace_event::TracedValue* traced_value) const {
  traced_value->BeginArray("compositing_reasons");
  for (const char* description :
       CompositingReason::Descriptions(compositing_reasons_))
    traced_value->AppendString(description);
  traced_value->EndArray();
}

void GraphicsLayerDebugInfo::AppendSquashingDisallowedReasons(
    base::trace_event::TracedValue* traced_value) const {
  traced_value->BeginArray("squashing_disallowed_reasons");
  for (const char* description :
       SquashingDisallowedReason::Descriptions(squashing_disallowed_reasons_))
    traced_value->AppendString(description);
  traced_value->EndArray();
}

void GraphicsLayerDebugInfo::AppendOwnerNodeId(
    base::trace_event::TracedValue* traced_value) const {
  if (!owner_node_id_)
    return;

  traced_value->SetInteger("owner_node", owner_node_id_);
}

void GraphicsLayerDebugInfo::AppendAnnotatedInvalidateRect(
    const FloatRect& rect,
    PaintInvalidationReason invalidation_reason) {
  AnnotatedInvalidationRect annotated_rect = {rect, invalidation_reason};
  invalidations_.push_back(annotated_rect);
}

void GraphicsLayerDebugInfo::ClearAnnotatedInvalidateRects() {
  previous_invalidations_.clear();
  previous_invalidations_.swap(invalidations_);
}

void GraphicsLayerDebugInfo::AppendMainThreadScrollingReasons(
    base::trace_event::TracedValue* traced_value) const {
  MainThreadScrollingReason::mainThreadScrollingReasonsAsTracedValue(
      main_thread_scrolling_reasons_, traced_value);
}

}  // namespace blink
