// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSTimingData_h
#define CSSTimingData_h

#include "platform/animation/TimingFunction.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"

namespace blink {

struct Timing;

class CSSTimingData {
  USING_FAST_MALLOC(CSSTimingData);

 public:
  ~CSSTimingData() {}

  const Vector<double>& DelayList() const { return delay_list_; }
  const Vector<double>& DurationList() const { return duration_list_; }
  const Vector<RefPtr<TimingFunction>>& TimingFunctionList() const {
    return timing_function_list_;
  }

  Vector<double>& DelayList() { return delay_list_; }
  Vector<double>& DurationList() { return duration_list_; }
  Vector<RefPtr<TimingFunction>>& TimingFunctionList() {
    return timing_function_list_;
  }

  static double InitialDelay() { return 0; }
  static double InitialDuration() { return 0; }
  static RefPtr<TimingFunction> InitialTimingFunction() {
    return CubicBezierTimingFunction::Preset(
        CubicBezierTimingFunction::EaseType::EASE);
  }

  template <class T>
  static const T& GetRepeated(const Vector<T>& v, size_t index) {
    return v[index % v.size()];
  }

 protected:
  CSSTimingData();
  explicit CSSTimingData(const CSSTimingData&);

  Timing ConvertToTiming(size_t index) const;
  bool TimingMatchForStyleRecalc(const CSSTimingData&) const;

 private:
  Vector<double> delay_list_;
  Vector<double> duration_list_;
  Vector<RefPtr<TimingFunction>> timing_function_list_;
};

}  // namespace blink

#endif  // CSSTimingData_h
