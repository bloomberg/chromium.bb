// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_ELEMENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_ELEMENT_TIMING_H_

#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ImageResourceContent;
class LayoutImage;
class PropertyTreeState;
class StyleImage;

// ImageElementTiming is responsible for tracking the paint timings for <img>
// elements for a given window.
class CORE_EXPORT ImageElementTiming final
    : public GarbageCollectedFinalized<ImageElementTiming>,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(ImageElementTiming);

 public:
  static const char kSupplementName[];

  explicit ImageElementTiming(LocalDOMWindow&);
  virtual ~ImageElementTiming() = default;

  static ImageElementTiming& From(LocalDOMWindow&);

  // Called when the LayoutObject has been painted. This method might queue a
  // swap promise to compute and report paint timestamps.
  void NotifyImagePainted(
      const LayoutObject*,
      const ImageResourceContent* cached_image,
      const PropertyTreeState& current_paint_chunk_properties);

  void NotifyBackgroundImagePainted(
      Node*,
      const StyleImage* background_image,
      const PropertyTreeState& current_paint_chunk_properties);

  // Called when the LayoutImage will be destroyed.
  void NotifyWillBeDestroyed(const LayoutObject*);

  void NotifyBackgroundImageRemoved(const LayoutObject*,
                                    const ImageResourceContent* image);

  void Trace(blink::Visitor*) override;

 private:
  friend class ImageElementTimingTest;

  void NotifyImagePaintedInternal(
      Node*,
      const LayoutObject&,
      const ImageResourceContent& cached_image,
      const PropertyTreeState& current_paint_chunk_properties);

  // Computes the intersection rect.
  FloatRect ComputeIntersectionRect(const LocalFrame*,
                                    const LayoutObject&,
                                    const PropertyTreeState&);
  // Checks if the element must be reported, given its elementtiming attribute
  // and its intersection rect.
  bool ShouldReportElement(const LocalFrame*,
                           const AtomicString& element_timing,
                           const FloatRect&) const;

  // Callback for the swap promise. Reports paint timestamps.
  void ReportImagePaintSwapTime(WebWidgetClient::SwapResult,
                                base::TimeTicks timestamp);

  // Class containing information about image element timing.
  class ElementTimingInfo
      : public GarbageCollectedFinalized<ElementTimingInfo> {
   public:
    ElementTimingInfo(const AtomicString& name,
                      const FloatRect& rect,
                      const TimeTicks& response_end,
                      const AtomicString& identifier,
                      const IntSize& intrinsic_size,
                      const AtomicString& id,
                      Element* element)
        : name(name),
          rect(rect),
          response_end(response_end),
          identifier(identifier),
          intrinsic_size(intrinsic_size),
          id(id),
          element(element) {}
    ~ElementTimingInfo() = default;

    void Trace(blink::Visitor* visitor) { visitor->Trace(element); }

    AtomicString name;
    FloatRect rect;
    TimeTicks response_end;
    AtomicString identifier;
    IntSize intrinsic_size;
    AtomicString id;
    Member<Element> element;

   private:
    DISALLOW_COPY_AND_ASSIGN(ElementTimingInfo);
  };

  // Vector containing the element timing infos that will be reported during the
  // next swap promise callback.
  HeapVector<Member<ElementTimingInfo>> element_timings_;
  // Hashmap of LayoutObjects for which paint has already been notified.
  WTF::HashSet<const LayoutObject*> images_notified_;
  // Hashmap of pairs of elements, background images whose paint has been
  // observed.
  WTF::HashSet<std::pair<const LayoutObject*, const ImageResourceContent*>>
      background_images_notified_;

  DISALLOW_COPY_AND_ASSIGN(ImageElementTiming);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_ELEMENT_TIMING_H_
