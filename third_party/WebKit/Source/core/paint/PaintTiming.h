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

namespace blink {

class LocalFrame;

// PaintTiming is responsible for tracking paint-related timings for a given
// document.
class CORE_EXPORT PaintTiming final
    : public GarbageCollectedFinalized<PaintTiming>,
      public Supplement<Document> {
  WTF_MAKE_NONCOPYABLE(PaintTiming);
  USING_GARBAGE_COLLECTED_MIXIN(PaintTiming);

 public:
  virtual ~PaintTiming() {}

  static PaintTiming& From(Document&);

  // mark*() methods record the time for the given paint event, record a trace
  // event, and notify that paint timing has changed. These methods do nothing
  // (early return) if a time has already been recorded for the given paint
  // event.
  void MarkFirstPaint();

  // markFirstTextPaint, markFirstImagePaint, and markFirstContentfulPaint
  // will also record first paint if first paint hasn't been recorded yet.
  void MarkFirstContentfulPaint();

  // markFirstTextPaint and markFirstImagePaint will also record first
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
  // monotonicallyIncreasingTime in wtf/CurrentTime.h for additional details.

  // firstPaint returns the first time that anything was painted for the
  // current document.
  double FirstPaint() const { return first_paint_; }

  // firstContentfulPaint returns the first time that 'contentful' content was
  // painted. For instance, the first time that text or image content was
  // painted.
  double FirstContentfulPaint() const { return first_contentful_paint_; }
  double FirstContentfulPaintSwap() const {
    return first_contentful_paint_swap_;
  }

  // firstTextPaint returns the first time that text content was painted.
  double FirstTextPaint() const { return first_text_paint_; }

  // firstImagePaint returns the first time that image content was painted.
  double FirstImagePaint() const { return first_image_paint_; }

  // firstMeaningfulPaint returns the first time that page's primary content
  // was painted.
  double FirstMeaningfulPaint() const { return first_meaningful_paint_; }
  double FirstMeaningfulPaintSwap() const {
    return first_meaningful_paint_swap_;
  }

  // firstMeaningfulPaintCandidate indicates the first time we considered a
  // paint to qualify as the potentially first meaningful paint. Unlike
  // firstMeaningfulPaint, this signal is available in real time, but it may be
  // an optimistic (i.e., too early) estimate.
  double FirstMeaningfulPaintCandidate() const {
    return first_meaningful_paint_candidate_;
  }

  FirstMeaningfulPaintDetector& GetFirstMeaningfulPaintDetector() {
    return *fmp_detector_;
  }

  void RegisterNotifySwapTime(PaintEvent,
                              WTF::Function<void(bool, double)> callback);
  void ReportSwapTime(PaintEvent, bool did_swap, double timestamp);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit PaintTiming(Document&);
  LocalFrame* GetFrame() const;
  void NotifyPaintTimingChanged();

  // set*() set the timing for the given paint event to the given timestamp
  // and record a trace event if the value is currently zero, but do not
  // notify that paint timing changed. These methods can be invoked from other
  // mark*() or set*() methods to make sure that first paint is marked as part
  // of marking first contentful paint, or that first contentful paint is
  // marked as part of marking first text/image paint, for example.
  void SetFirstPaint(double stamp);

  // setFirstContentfulPaint will also set first paint time if first paint
  // time has not yet been recorded.
  void SetFirstContentfulPaint(double stamp);

  void RegisterNotifySwapTime(PaintEvent);
  void ReportUserInputHistogram(
      FirstMeaningfulPaintDetector::HadUserInput had_input);

  double first_paint_ = 0.0;
  double first_paint_swap_ = 0.0;
  double first_text_paint_ = 0.0;
  double first_image_paint_ = 0.0;
  double first_contentful_paint_ = 0.0;
  double first_contentful_paint_swap_ = 0.0;
  double first_meaningful_paint_ = 0.0;
  double first_meaningful_paint_swap_ = 0.0;
  double first_meaningful_paint_candidate_ = 0.0;

  Member<FirstMeaningfulPaintDetector> fmp_detector_;
};

}  // namespace blink

#endif
