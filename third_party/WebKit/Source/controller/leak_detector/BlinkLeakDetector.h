// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlinkLeakDetector_h
#define BlinkLeakDetector_h

#include "controller/ControllerExport.h"
#include "platform/Timer.h"

namespace blink {

class BlinkLeakDetectorClient;
class LocalFrame;
class WebFrame;

// This class is responsible for stabilizing the detection results which are
// InstanceCounter values by 1) preparing for leak detection and 2) operating
// garbage collections before leak detection.
class CONTROLLER_EXPORT BlinkLeakDetector {
 public:
  BlinkLeakDetector(BlinkLeakDetectorClient*, WebFrame*);
  ~BlinkLeakDetector();

  void PrepareForLeakDetection();
  void CollectGarbage();

 private:
  void TimerFiredGC(TimerBase*);

  TaskRunnerTimer<BlinkLeakDetector> delayed_gc_timer_;
  int number_of_gc_needed_;
  BlinkLeakDetectorClient* client_;
  Persistent<LocalFrame> frame_;
};
}  // namespace blink

#endif  // LeakDetector_h
