// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PerformanceLongTaskTiming_h
#define PerformanceLongTaskTiming_h

#include "core/timing/PerformanceEntry.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class TaskAttributionTiming;
using TaskAttributionVector = HeapVector<Member<TaskAttributionTiming>>;

class PerformanceLongTaskTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PerformanceLongTaskTiming* create(double startTime,
                                           double endTime,
                                           String name,
                                           String frameSrc,
                                           String frameId,
                                           String frameName);

  TaskAttributionVector attribution() const;

  DECLARE_VIRTUAL_TRACE();

 private:
  PerformanceLongTaskTiming(double startTime,
                            double endTime,
                            String name,
                            String frameSrc,
                            String frameId,
                            String frameName);
  ~PerformanceLongTaskTiming() override;

  TaskAttributionVector m_attribution;
};

}  // namespace blink

#endif  // PerformanceLongTaskTiming_h
