// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintTiming_h
#define PaintTiming_h

#include <memory>

#include "core/dom/Document.h"
#include "core/paint/FirstMeaningfulPaintDetector.h"
#include "core/paint/PaintEvent.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebLayerTreeView.h"

namespace blink {

class LocalFrame;

// PaintTiming is responsible for tracking paint-related timings for a given
// document.
class CORE_EXPORT PaintTiming final
    : public GarbageCollectedFinalized<PaintTiming>,
      public Supplement<Document> {
  WTF_MAKE_NONCOPYABLE(PaintTiming);
  USING_GARBAGE_COLLECTED_MIXIN(PaintTiming);
  friend class FirstMeaningfulPaintDetector;
  using ReportTimeCallback =
      WTF::CrossThreadFunction<void(WebLayerTreeView::SwapResult, double)>;

 public:
  virtual ~PaintTiming() {}

  static PaintTiming& From(Document&);

  // Mark*() methods record the time for the given paint event and queue a swap
  // promise to record the |first_*_swap_| timestamp. These methods do nothing
  // (early return) if a time has already been recorded for the given paint
  // event.
  void MarkFirstPaint();

  // MarkFirstTextPaint, MarkFirstImagePaint, and MarkFirstContentfulPaint
  // will also record first paint if first paint hasn't been recorded yet.
  void MarkFirstContentfulPaint();

  // MarkFirstTextPaint and MarkFirstImagePaint will also record first
  // contentful paint if first contentful paint hasn't been recorded yet.
  void MarkFirstTextPaint();
  void MarkFirstImagePaint();

  void SetFirstMeaningfulPaintCandidate(double timestamp);
  void SetFirstMeaningfulPaint(
      double stamp,
      double swap_stamp,
      FirstMeaningfulPaintDetector::HadUserInput had_input);
  void NotifyPaint(bool is_first_paint, bool text_painted, bool image_painted);

  // The getters below return monotonically-increasing seconds, or zero if the
  // given paint event has not yet occurred. See the comments for
  // monotonicallyIncreasingTime in wtf/Time.h for additional details.

  // FirstPaint returns the first time that anything was painted for the
  // current document.
  double FirstPaint() const { return first_paint_swap_; }

  // FirstContentfulPaint returns the first time that 'contentful' content was
  // painted. For instance, the first time that text or image content was
  // painted.
  double FirstContentfulPaint() const { return first_contentful_paint_swap_; }

  // FirstTextPaint returns the first time that text content was painted.
  double FirstTextPaint() const { return first_text_paint_swap_; }

  // FirstImagePaint returns the first time that image content was painted.
  double FirstImagePaint() const { return first_image_paint_swap_; }

  // FirstMeaningfulPaint returns the first time that page's primary content
  // was painted.
  double FirstMeaningfulPaint() const { return first_meaningful_paint_swap_; }

  // FirstMeaningfulPaintCandidate indicates the first time we considered a
  // paint to qualify as the potentially first meaningful paint. Unlike
  // firstMeaningfulPaint, this signal is available in real time, but it may be
  // an optimistic (i.e., too early) estimate.
  double FirstMeaningfulPaintCandidate() const {
    return first_meaningful_paint_candidate_;
  }

  FirstMeaningfulPaintDetector& GetFirstMeaningfulPaintDetector() {
    return *fmp_detector_;
  }

  void RegisterNotifySwapTime(PaintEvent, ReportTimeCallback);
  void ReportSwapTime(PaintEvent,
                      WebLayerTreeView::SwapResult,
                      double timestamp);

  void ReportSwapResultHistogram(const WebLayerTreeView::SwapResult);

  void Trace(blink::Visitor*) override;

 private:
  explicit PaintTiming(Document&);
  LocalFrame* GetFrame() const;
  void NotifyPaintTimingChanged();

  // Set*() set the timing for the given paint event to the given timestamp if
  // the value is currently zero, and queue a swap promise to record the
  // |first_*_swap_| timestamp. These methods can be invoked from other Mark*()
  // or Set*() methods to make sure that first paint is marked as part of
  // marking first contentful paint, or that first contentful paint is marked as
  // part of marking first text/image paint, for example.
  void SetFirstPaint(double stamp);

  // setFirstContentfulPaint will also set first paint time if first paint
  // time has not yet been recorded.
  void SetFirstContentfulPaint(double stamp);

  // Set*Swap() are called when the SwapPromise is fulfilled and the swap
  // timestamp is available. These methods will record trace events, update Web
  // Perf API (FP and FCP only), and notify that paint timing has changed, which
  // triggers UMAs and UKMS.
  // |stamp| is the swap timestamp used for tracing, UMA, UKM, and Web Perf API.
  void SetFirstPaintSwap(double stamp);
  void SetFirstContentfulPaintSwap(double stamp);
  void SetFirstImagePaintSwap(double stamp);
  void SetFirstTextPaintSwap(double stamp);

  void RegisterNotifySwapTime(PaintEvent);
  void ReportUserInputHistogram(
      FirstMeaningfulPaintDetector::HadUserInput had_input);
  void ReportSwapTimeDeltaHistogram(double timestamp, double swap_timestamp);

  double FirstPaintRendered() const { return first_paint_; }

  double FirstContentfulPaintRendered() const {
    return first_contentful_paint_;
  }

  double FirstMeaningfulPaintRendered() const {
    return first_meaningful_paint_;
  }

  // TODO(crbug/738235): Non first_*_swap_ variables are only being tracked to
  // compute deltas for reporting histograms and should be removed once we
  // confirm the deltas and discrepancies look reasonable.
  double first_paint_ = 0.0;
  double first_paint_swap_ = 0.0;
  double first_text_paint_ = 0.0;
  double first_text_paint_swap_ = 0.0;
  double first_image_paint_ = 0.0;
  double first_image_paint_swap_ = 0.0;
  double first_contentful_paint_ = 0.0;
  double first_contentful_paint_swap_ = 0.0;
  double first_meaningful_paint_ = 0.0;
  double first_meaningful_paint_swap_ = 0.0;
  double first_meaningful_paint_candidate_ = 0.0;

  Member<FirstMeaningfulPaintDetector> fmp_detector_;

  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest, NoFirstPaint);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest, OneLayout);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           TwoLayoutsSignificantSecond);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           TwoLayoutsSignificantFirst);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           FirstMeaningfulPaintCandidate);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      OnlyOneFirstMeaningfulPaintCandidateBeforeNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           NetworkStableBeforeFirstContentfulPaint);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      FirstMeaningfulPaintShouldNotBeBeforeFirstContentfulPaint);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network2QuietThen0Quiet);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network0QuietThen2Quiet);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network0QuietTimer);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           Network2QuietTimer);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           FirstMeaningfulPaintAfterUserInteraction);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           UserInteractionBeforeFirstPaint);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      WaitForSingleOutstandingSwapPromiseAfterNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      WaitForMultipleOutstandingSwapPromisesAfterNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(FirstMeaningfulPaintDetectorTest,
                           WaitForFirstContentfulPaintSwapAfterNetworkStable);
  FRIEND_TEST_ALL_PREFIXES(
      FirstMeaningfulPaintDetectorTest,
      ProvisionalTimestampChangesAfterNetworkQuietWithOutstandingSwapPromise);
};

}  // namespace blink

#endif
