// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FirstMeaningfulPaintDetector_h
#define FirstMeaningfulPaintDetector_h

#include "core/CoreExport.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

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
    void reset() { m_count = 0; }
    void increment() { m_count++; }
    unsigned count() const { return m_count; }

   private:
    unsigned m_count = 0;
  };

  static FirstMeaningfulPaintDetector& from(Document&);

  FirstMeaningfulPaintDetector(PaintTiming*, Document&);
  virtual ~FirstMeaningfulPaintDetector() {}

  void markNextPaintAsMeaningfulIfNeeded(const LayoutObjectCounter&,
                                         int contentsHeightBeforeLayout,
                                         int contentsHeightAfterLayout,
                                         int visibleHeight);
  void notifyPaint();
  void checkNetworkStable();

  DECLARE_TRACE();

 private:
  friend class FirstMeaningfulPaintDetectorTest;

  // The page is n-quiet if there are no more than n active network requests for
  // this duration of time.
  static constexpr double kNetwork2QuietWindowSeconds = 0.5;
  static constexpr double kNetwork0QuietWindowSeconds = 0.5;

  Document* document();
  int activeConnections();
  void setNetworkQuietTimers(int activeConnections);
  void network0QuietTimerFired(TimerBase*);
  void network2QuietTimerFired(TimerBase*);
  void reportHistograms();

  bool m_nextPaintIsMeaningful = false;

  Member<PaintTiming> m_paintTiming;
  double m_provisionalFirstMeaningfulPaint = 0.0;
  double m_maxSignificanceSoFar = 0.0;
  double m_accumulatedSignificanceWhileHavingBlankText = 0.0;
  unsigned m_prevLayoutObjectCount = 0;
  bool m_seenFirstMeaningfulPaintCandidate = false;
  bool m_network0QuietReached = false;
  bool m_network2QuietReached = false;
  double m_firstMeaningfulPaint0Quiet = 0.0;
  double m_firstMeaningfulPaint2Quiet = 0.0;
  TaskRunnerTimer<FirstMeaningfulPaintDetector> m_network0QuietTimer;
  TaskRunnerTimer<FirstMeaningfulPaintDetector> m_network2QuietTimer;
};

}  // namespace blink

#endif
