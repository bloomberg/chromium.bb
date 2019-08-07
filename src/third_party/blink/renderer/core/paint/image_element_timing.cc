// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_element_timing.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_replaced.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/graphics/paint/property_tree_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// TODO(npm): decide on a reasonable value for the threshold.
constexpr const float kImageTimingSizeThreshold = 0.15f;

// The maximum amount of characters included in Element Timing for inline
// images.
constexpr const unsigned kInlineImageMaxChars = 100u;

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
  DCHECK(RuntimeEnabledFeatures::ElementTimingEnabled(
      GetSupplementable()->document()));
}

void ImageElementTiming::NotifyImagePainted(
    const LayoutObject* layout_object,
    const ImageResourceContent* cached_image,
    const PropertyTreeState& current_paint_chunk_properties) {
  DCHECK(layout_object);
  auto result = images_notified_.insert(layout_object);
  if (result.is_new_entry && cached_image) {
    NotifyImagePaintedInternal(layout_object->GetNode(), *layout_object,
                               *cached_image, current_paint_chunk_properties);
  }
}

void ImageElementTiming::NotifyImagePaintedInternal(
    Node* node,
    const LayoutObject& layout_object,
    const ImageResourceContent& cached_image,
    const PropertyTreeState& current_paint_chunk_properties) {
  LocalFrame* frame = GetSupplementable()->GetFrame();
  DCHECK(frame == layout_object.GetDocument().GetFrame());
  DCHECK(node);
  // Background images could cause |node| to not be an element. For example,
  // style applied to body causes this node to be a Document Node. Therefore,
  // bail out if that is the case.
  if (!frame || !node->IsElementNode())
    return;

  // We do not expose elements in shadow trees, for now. We might expose
  // something once the discussions at
  // https://github.com/WICG/element-timing/issues/3 and
  // https://github.com/w3c/webcomponents/issues/816 have been resolved.
  if (node->IsInShadowTree())
    return;

  FloatRect intersection_rect = ComputeIntersectionRect(
      frame, layout_object, current_paint_chunk_properties);
  Element* element = ToElement(node);
  const AtomicString attr =
      element->FastGetAttribute(html_names::kElementtimingAttr);
  if (!ShouldReportElement(frame, attr, intersection_rect))
    return;

  const AtomicString& id = element->GetIdAttribute();

  const KURL& url = cached_image.Url();
  DCHECK(GetSupplementable()->document() == &layout_object.GetDocument());
  DCHECK(layout_object.GetDocument().GetSecurityOrigin());
  // It's ok to expose rendering timestamp for data URIs so exclude those from
  // the Timing-Allow-Origin check.
  if (!url.ProtocolIsData() &&
      !Performance::PassesTimingAllowCheck(
          cached_image.GetResponse(),
          *layout_object.GetDocument().GetSecurityOrigin(),
          &layout_object.GetDocument())) {
    WindowPerformance* performance =
        DOMWindowPerformance::performance(*GetSupplementable());
    if (performance &&
        (performance->HasObserverFor(PerformanceEntry::kElement) ||
         performance->ShouldBufferEntries())) {
      // Create an entry with a |startTime| of 0.
      performance->AddElementTiming(
          AtomicString(url.GetString()), intersection_rect, TimeTicks(),
          cached_image.LoadResponseEnd(), attr,
          cached_image.IntrinsicSize(kDoNotRespectImageOrientation), id,
          element);
    }
    return;
  }

  // If the image URL is a data URL ("data:image/..."), then the |name| of the
  // PerformanceElementTiming entry should be the URL trimmed to 100 characters.
  // If it is not, then pass in the full URL regardless of the length to be
  // consistent with Resource Timing.
  const String& image_name = url.ProtocolIsData()
                                 ? url.GetString().Left(kInlineImageMaxChars)
                                 : url.GetString();
  element_timings_.emplace_back(MakeGarbageCollected<ElementTimingInfo>(
      AtomicString(image_name), intersection_rect,
      cached_image.LoadResponseEnd(), attr,
      cached_image.IntrinsicSize(kDoNotRespectImageOrientation), id, element));
  // Only queue a swap promise when |element_timings_| was empty. All of the
  // records in |element_timings_| will be processed when the promise succeeds
  // or fails, and at that time the vector is cleared.
  if (element_timings_.size() == 1) {
    frame->GetChromeClient().NotifySwapTime(
        *frame,
        CrossThreadBindOnce(&ImageElementTiming::ReportImagePaintSwapTime,
                            WrapCrossThreadWeakPersistent(this)));
  }
}

void ImageElementTiming::NotifyBackgroundImagePainted(
    Node* node,
    const StyleImage* background_image,
    const PropertyTreeState& current_paint_chunk_properties) {
  DCHECK(node);
  DCHECK(background_image);
  const LayoutObject* layout_object = node->GetLayoutObject();
  if (!layout_object)
    return;

  const ImageResourceContent* cached_image = background_image->CachedImage();
  if (!cached_image || !cached_image->IsLoaded())
    return;

  auto result = background_images_notified_.insert(
      std::make_pair(layout_object, cached_image));
  if (result.is_new_entry) {
    NotifyImagePaintedInternal(node, *layout_object, *cached_image,
                               current_paint_chunk_properties);
  }
}

FloatRect ImageElementTiming::ComputeIntersectionRect(
    const LocalFrame* frame,
    const LayoutObject& layout_object,
    const PropertyTreeState& current_paint_chunk_properties) {
  // Compute the visible part of the image rect.
  IntRect image_visual_rect = layout_object.FirstFragment().VisualRect();

  FloatClipRect visual_rect = FloatClipRect(FloatRect(image_visual_rect));
  GeometryMapper::LocalToAncestorVisualRect(current_paint_chunk_properties,
                                            frame->View()
                                                ->GetLayoutView()
                                                ->FirstFragment()
                                                .LocalBorderBoxProperties(),
                                            visual_rect);
  return visual_rect.Rect();
}

bool ImageElementTiming::ShouldReportElement(
    const LocalFrame* frame,
    const AtomicString& element_timing,
    const FloatRect& intersection_rect) const {
  // Create an entry if 'elementtiming' is present or if the fraction of the
  // viewport occupied by the image is above a certain size threshold.
  return !element_timing.IsEmpty() ||
         intersection_rect.Size().Area() > frame->View()
                                                   ->LayoutViewport()
                                                   ->VisibleContentRect()
                                                   .Size()
                                                   .Area() *
                                               kImageTimingSizeThreshold;
}

void ImageElementTiming::ReportImagePaintSwapTime(WebWidgetClient::SwapResult,
                                                  base::TimeTicks timestamp) {
  WindowPerformance* performance =
      DOMWindowPerformance::performance(*GetSupplementable());
  if (performance && (performance->HasObserverFor(PerformanceEntry::kElement) ||
                      performance->ShouldBufferEntries())) {
    for (const auto& element_timing : element_timings_) {
      performance->AddElementTiming(
          element_timing->name, element_timing->rect, timestamp,
          element_timing->response_end, element_timing->identifier,
          element_timing->intrinsic_size, element_timing->id,
          element_timing->element);
    }
  }
  element_timings_.clear();
}

void ImageElementTiming::NotifyWillBeDestroyed(const LayoutObject* image) {
  images_notified_.erase(image);
}

void ImageElementTiming::NotifyBackgroundImageRemoved(
    const LayoutObject* layout_object,
    const ImageResourceContent* image) {
  background_images_notified_.erase(std::make_pair(layout_object, image));
}

void ImageElementTiming::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_timings_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
