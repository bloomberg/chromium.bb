// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_DETECTOR_H_

#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Image;
class ImagePaintTimingDetector;
class ImageResourceContent;
class LayoutObject;
class LocalFrameView;
class PropertyTreeState;
class TextPaintTimingDetector;

// PaintTimingDetector contains some of paint metric detectors,
// providing common infrastructure for these detectors.
//
// Users has to enable 'loading' trace category to enable the metrics.
//
// See also:
// https://docs.google.com/document/d/1DRVd4a2VU8-yyWftgOparZF-sf16daf0vfbsHuz2rws/edit
class CORE_EXPORT PaintTimingDetector
    : public GarbageCollected<PaintTimingDetector> {
 public:
  PaintTimingDetector(LocalFrameView*);

  static void NotifyBackgroundImagePaint(
      const Node*,
      const Image*,
      const ImageResourceContent* cached_image,
      const PropertyTreeState& current_paint_chunk_properties);
  static void NotifyImagePaint(
      const LayoutObject&,
      const IntSize& intrinsic_size,
      const ImageResourceContent* cached_image,
      const PropertyTreeState& current_paint_chunk_properties);

  static void NotifyTextPaint(const LayoutObject&, const PropertyTreeState&);
  void NotifyNodeRemoved(const LayoutObject&);
  void NotifyBackgroundImageRemoved(const LayoutObject&,
                                    const ImageResourceContent*);
  void NotifyPaintFinished();
  void NotifyInputEvent(WebInputEvent::Type);
  bool NeedToNotifyInputOrScroll();
  void NotifyScroll(ScrollType);
  void NotifyLargestImage(base::TimeTicks, uint64_t size);
  void NotifyLargestText(base::TimeTicks, uint64_t size);
  void DidChangePerformanceTiming();

  // |visual_rect| should be an object's bounding rect in the space of
  // PropertyTreeState.
  uint64_t CalculateVisualSize(const IntRect& visual_rect,
                               const PropertyTreeState&) const;

  TextPaintTimingDetector* GetTextPaintTimingDetector() {
    return text_paint_timing_detector_;
  }
  ImagePaintTimingDetector* GetImagePaintTimingDetector() {
    return image_paint_timing_detector_;
  }
  base::TimeTicks LargestImagePaint() const {
    return largest_image_paint_time_;
  }
  uint64_t LargestImagePaintSize() const { return largest_image_paint_size_; }
  base::TimeTicks LargestTextPaint() const { return largest_text_paint_time_; }
  uint64_t LargestTextPaintSize() const { return largest_text_paint_size_; }
  void Trace(Visitor* visitor);

 private:
  Member<LocalFrameView> frame_view_;
  // This member lives until the end of the paint phase after the largest text
  // paint is found.
  Member<TextPaintTimingDetector> text_paint_timing_detector_;
  // This member lives until the end of the paint phase after the largest image
  // paint is found.
  Member<ImagePaintTimingDetector> image_paint_timing_detector_;

  // Largest image information.
  base::TimeTicks largest_image_paint_time_;
  uint64_t largest_image_paint_size_ = 0;
  // Largest text information.
  base::TimeTicks largest_text_paint_time_;
  uint64_t largest_text_paint_size_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_TIMING_DETECTOR_H_
