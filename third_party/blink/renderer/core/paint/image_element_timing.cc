// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_element_timing.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// TODO(npm): decide on a reasonable value for the threshold.
constexpr const float kImageTimingSizeThreshold = 0.15f;

// static
const char ImageElementTiming::kSupplementName[] = "ImageElementTiming";

// static
ImageElementTiming& ImageElementTiming::From(LocalDOMWindow& window) {
  ImageElementTiming* timing =
      Supplement<LocalDOMWindow>::From<ImageElementTiming>(window);
  if (!timing) {
    timing = MakeGarbageCollected<ImageElementTiming>(window);
    ProvideTo(window, timing);
  }
  return *timing;
}

ImageElementTiming::ImageElementTiming(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {
  DCHECK(origin_trials::ElementTimingEnabled(GetSupplementable()->document()));
}

void ImageElementTiming::NotifyImagePainted(
    const LayoutObject* layout_object,
    const ImageResourceContent* cached_image,
    const PaintLayer* painting_layer) {
  auto result = images_notified_.insert(layout_object);
  if (!result.is_new_entry || !cached_image)
    return;

  LocalFrame* frame = GetSupplementable()->GetFrame();
  DCHECK(frame == layout_object->GetDocument().GetFrame());
  if (!frame)
    return;

  IntRect intersection_rect =
      ComputeIntersectionRect(frame, layout_object, painting_layer);
  const Element* element = ToElement(layout_object->GetNode());
  const AtomicString attr =
      element->FastGetAttribute(html_names::kElementtimingAttr);
  if (!ShouldReportElement(attr, intersection_rect))
    return;

  DCHECK(GetSupplementable()->document() == &layout_object->GetDocument());
  DCHECK(layout_object->GetDocument().GetSecurityOrigin());
  if (!Performance::PassesTimingAllowCheck(
          cached_image->GetResponse(),
          *layout_object->GetDocument().GetSecurityOrigin(),
          &layout_object->GetDocument())) {
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*GetSupplementable());
    if (performance &&
        (performance->HasObserverFor(PerformanceEntry::kElement) ||
         performance->ShouldBufferEntries())) {
      // Create an entry with a |startTime| of 0.
      performance->AddElementTiming(
          AtomicString(cached_image->Url().GetString()), intersection_rect,
          TimeTicks(), cached_image->LoadResponseEnd(), attr);
    }
    return;
  }

  WebLayerTreeView* layerTreeView =
      frame->GetChromeClient().GetWebLayerTreeView(frame);
  if (!layerTreeView)
    return;

  element_timings_.emplace_back(AtomicString(cached_image->Url().GetString()),
                                intersection_rect,
                                cached_image->LoadResponseEnd(), attr);
  // Only queue a swap promise when |element_timings_| was empty. All of the
  // records in |element_timings_| will be processed when the promise succeeds
  // or fails, and at that time the vector is cleared.
  if (element_timings_.size() == 1) {
    layerTreeView->NotifySwapTime(ConvertToBaseCallback(
        CrossThreadBind(&ImageElementTiming::ReportImagePaintSwapTime,
                        WrapCrossThreadWeakPersistent(this))));
  }
}

IntRect ImageElementTiming::ComputeIntersectionRect(
    const LocalFrame* frame,
    const LayoutObject* layout_object,
    const PaintLayer* painting_layer) {
  viewport_ = frame->View()->LayoutViewport()->VisibleContentRect();

  // Compute the visible part of the image rect.
  LayoutRect image_visual_rect = layout_object->FirstFragment().VisualRect();
  const auto& local_transform = painting_layer->GetLayoutObject()
                                    .FirstFragment()
                                    .LocalBorderBoxProperties()
                                    .Transform();
  const auto& ancestor_transform = painting_layer->GetLayoutObject()
                                       .View()
                                       ->FirstFragment()
                                       .LocalBorderBoxProperties()
                                       .Transform();
  FloatRect new_visual_rect_abs = FloatRect(image_visual_rect);
  GeometryMapper::SourceToDestinationRect(local_transform, ancestor_transform,
                                          new_visual_rect_abs);
  IntRect visible_new_visual_rect = RoundedIntRect(new_visual_rect_abs);
  visible_new_visual_rect.Intersect(viewport_);
  return visible_new_visual_rect;
}

bool ImageElementTiming::ShouldReportElement(
    const AtomicString& element_timing,
    const IntRect& intersection_rect) const {
  // Do not create an entry if 'elementtiming' is not present or the image is
  // below a certain size threshold.
  return !element_timing.IsEmpty() ||
         intersection_rect.Size().Area() >
             viewport_.Size().Area() * kImageTimingSizeThreshold;
}

void ImageElementTiming::ReportImagePaintSwapTime(WebLayerTreeView::SwapResult,
                                                  base::TimeTicks timestamp) {
  WindowPerformance* performance =
      DOMWindowPerformance::performance(*GetSupplementable());
  if (performance && (performance->HasObserverFor(PerformanceEntry::kElement) ||
                      performance->ShouldBufferEntries())) {
    for (const auto& element_timing : element_timings_) {
      performance->AddElementTiming(element_timing.name, element_timing.rect,
                                    timestamp, element_timing.response_end,
                                    element_timing.identifier);
    }
  }
  element_timings_.clear();
}

void ImageElementTiming::NotifyWillBeDestroyed(const LayoutObject* image) {
  images_notified_.erase(image);
}

void ImageElementTiming::Trace(blink::Visitor* visitor) {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
