// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FirstMeaningfulPaintDetector_h
#define FirstMeaningfulPaintDetector_h

#include "core/CoreExport.h"
#include "core/paint/PaintEvent.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebLayerTreeView.h"

namespace blink {

class Document;
class PaintTiming;

// FirstMeaningfulPaintDetector observes layout operations during page load
// until network stable (2 seconds of no network activity), and computes the
// layout-based First Meaningful Paint.
// See https://goo.gl/vpaxv6 and http://goo.gl/TEiMi4 for more details.
class CORE_EXPORT FirstMeaningfulPaintDetector
    : public GarbageCollectedFinalized<FirstMeaningfulPaintDetector> {
  WTF_MAKE_NONCOPYABLE(FirstMeaningfulPaintDetector);

 public:
  // Used by FrameView to keep track of the number of layout objects created
  // in the frame.
  class LayoutObjectCounter {
   public:
    void Reset() { count_ = 0; }
    void Increment() { count_++; }
    unsigned Count() const { return count_; }

   private:
    unsigned count_ = 0;
  };

  static FirstMeaningfulPaintDetector& From(Document&);

  FirstMeaningfulPaintDetector(PaintTiming*, Document&);
  virtual ~FirstMeaningfulPaintDetector() {}

  void MarkNextPaintAsMeaningfulIfNeeded(const LayoutObjectCounter&,
                                         int contents_height_before_layout,
                                         int contents_height_after_layout,
                                         int visible_height);
  void NotifyInputEvent();
  void NotifyPaint();
  void CheckNetworkStable();
  void ReportSwapTime(PaintEvent, WebLayerTreeView::SwapResult, double);
  void NotifyFirstContentfulPaint(double swap_stamp);

  void Trace(blink::Visitor*);

  enum HadUserInput { kNoUserInput, kHadUserInput, kHadUserInputEnumMax };

 private:
  friend class FirstMeaningfulPaintDetectorTest;

  enum DeferFirstMeaningfulPaint {
    kDoNotDefer,
    kDeferOutstandingSwapPromises,
    kDeferFirstContentfulPaintNotSet
  };

  // The page is n-quiet if there are no more than n active network requests for
  // this duration of time.
  static constexpr double kNetwork2QuietWindowSeconds = 0.5;
  static constexpr double kNetwork0QuietWindowSeconds = 0.5;

  Document* GetDocument();
  int ActiveConnections();
  void SetNetworkQuietTimers(int active_connections);
  void Network0QuietTimerFired(TimerBase*);
  void Network2QuietTimerFired(TimerBase*);
  void ReportHistograms();
  void RegisterNotifySwapTime(PaintEvent);
  void SetFirstMeaningfulPaint(double stamp, double swap_stamp);

  bool next_paint_is_meaningful_ = false;
  HadUserInput had_user_input_ = kNoUserInput;
  HadUserInput had_user_input_before_provisional_first_meaningful_paint_ =
      kNoUserInput;

  Member<PaintTiming> paint_timing_;
  double provisional_first_meaningful_paint_ = 0.0;
  double provisional_first_meaningful_paint_swap_ = 0.0;
  double max_significance_so_far_ = 0.0;
  double accumulated_significance_while_having_blank_text_ = 0.0;
  unsigned prev_layout_object_count_ = 0;
  bool seen_first_meaningful_paint_candidate_ = false;
  bool network0_quiet_reached_ = false;
  bool network2_quiet_reached_ = false;
  double first_meaningful_paint0_quiet_ = 0.0;
  double first_meaningful_paint2_quiet_ = 0.0;
  unsigned outstanding_swap_promise_count_ = 0;
  DeferFirstMeaningfulPaint defer_first_meaningful_paint_ = kDoNotDefer;
  TaskRunnerTimer<FirstMeaningfulPaintDetector> network0_quiet_timer_;
  TaskRunnerTimer<FirstMeaningfulPaintDetector> network2_quiet_timer_;
};

}  // namespace blink

#endif
