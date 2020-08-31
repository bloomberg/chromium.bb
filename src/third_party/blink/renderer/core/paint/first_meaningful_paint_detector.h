// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIRST_MEANINGFUL_PAINT_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIRST_MEANINGFUL_PAINT_DETECTOR_H_

#include "base/macros.h"
#include "third_party/blink/public/web/web_swap_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace base {
class TickClock;
}

namespace blink {

class Document;
class LayoutObjectCounter;
class PaintTiming;

// FirstMeaningfulPaintDetector observes layout operations during page load
// until network stable (no more than 2 network connections active in 0.5
// seconds), and computes the layout-based First Meaningful Paint.
// See https://goo.gl/vpaxv6 and http://goo.gl/TEiMi4 for more details.
class CORE_EXPORT FirstMeaningfulPaintDetector
    : public GarbageCollected<FirstMeaningfulPaintDetector> {
 public:
  static FirstMeaningfulPaintDetector& From(Document&);

  explicit FirstMeaningfulPaintDetector(PaintTiming*);
  virtual ~FirstMeaningfulPaintDetector() = default;

  void MarkNextPaintAsMeaningfulIfNeeded(const LayoutObjectCounter&,
                                         double contents_height_before_layout,
                                         double contents_height_after_layout,
                                         int visible_height);
  void NotifyInputEvent();
  void NotifyPaint();
  void ReportSwapTime(PaintEvent, WebSwapResult, base::TimeTicks);
  void NotifyFirstContentfulPaint(base::TimeTicks swap_stamp);
  void OnNetwork2Quiet();
  bool SeenFirstMeaningfulPaint() const;

  // The caller owns the |clock| which must outlive the paint detector.
  static void SetTickClockForTesting(const base::TickClock* clock);

  void Trace(Visitor*);

  enum HadUserInput { kNoUserInput, kHadUserInput, kHadUserInputEnumMax };

 private:
  friend class FirstMeaningfulPaintDetectorTest;

  enum DeferFirstMeaningfulPaint {
    kDoNotDefer,
    kDeferOutstandingSwapPromises,
    kDeferFirstContentfulPaintNotSet
  };

  Document* GetDocument();
  int ActiveConnections();
  void RegisterNotifySwapTime(PaintEvent);
  void SetFirstMeaningfulPaint(base::TimeTicks swap_stamp);

  bool next_paint_is_meaningful_ = false;
  HadUserInput had_user_input_ = kNoUserInput;
  HadUserInput had_user_input_before_provisional_first_meaningful_paint_ =
      kNoUserInput;

  Member<PaintTiming> paint_timing_;
  base::TimeTicks provisional_first_meaningful_paint_;
  base::TimeTicks provisional_first_meaningful_paint_swap_;
  double max_significance_so_far_ = 0.0;
  double accumulated_significance_while_having_blank_text_ = 0.0;
  unsigned prev_layout_object_count_ = 0;
  bool seen_first_meaningful_paint_candidate_ = false;
  bool network_quiet_reached_ = false;
  base::TimeTicks first_meaningful_paint_;
  unsigned outstanding_swap_promise_count_ = 0;
  DeferFirstMeaningfulPaint defer_first_meaningful_paint_ = kDoNotDefer;
  DISALLOW_COPY_AND_ASSIGN(FirstMeaningfulPaintDetector);
};

}  // namespace blink

#endif
