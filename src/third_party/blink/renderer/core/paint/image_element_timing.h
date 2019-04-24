// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_ELEMENT_TIMING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_IMAGE_ELEMENT_TIMING_H_

#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ImageResourceContent;
class LayoutImage;

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
  void NotifyImagePainted(const LayoutObject*,
                          const ImageResourceContent* cached_image,
                          const PaintLayer*);

  // Called when the LayoutImage will be destroyed.
  void NotifyWillBeDestroyed(const LayoutObject*);

  void Trace(blink::Visitor*) override;

 private:
  friend class ImageElementTimingTest;
  // Computes the intersection rect and stores the viewport rect in |viewport_|.
  IntRect ComputeIntersectionRect(const LocalFrame*,
                                  const LayoutObject*,
                                  const PaintLayer*);
  // Checks if the element must be reported, given its elementtiming attribute
  // and its intersection rect. Assumes that |viewport_| has been calculated,
  // i.e. that ComputeIntersectionRect() has been called.
  bool ShouldReportElement(const AtomicString& element_timing,
                           const IntRect&) const;

  // Callback for the swap promise. Reports paint timestamps.
  void ReportImagePaintSwapTime(WebLayerTreeView::SwapResult,
                                base::TimeTicks timestamp);

  // Struct containing information about image element timing.
  struct ElementTimingInfo {
    ElementTimingInfo(const AtomicString& name,
                      IntRect rect,
                      TimeTicks response_end,
                      const AtomicString& identifier)
        : name(name),
          rect(rect),
          response_end(response_end),
          identifier(identifier) {}

    AtomicString name;
    IntRect rect;
    TimeTicks response_end;
    AtomicString identifier;
  };
  // Vector containing the element timing infos that will be reported during the
  // next swap promise callback.
  WTF::Vector<ElementTimingInfo> element_timings_;
  // Hashmap of LayoutObjects for which paint has already been notified.
  WTF::HashSet<const LayoutObject*> images_notified_;
  // Current viewport rect, updated every time an intersection rect is computed.
  IntRect viewport_;

  DISALLOW_COPY_AND_ASSIGN(ImageElementTiming);
};

}  // namespace blink

#endif
