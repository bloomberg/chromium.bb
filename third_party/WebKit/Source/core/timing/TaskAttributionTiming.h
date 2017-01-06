// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskAttributionTiming_h
#define TaskAttributionTiming_h

#include "core/timing/PerformanceEntry.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class TaskAttributionTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static TaskAttributionTiming* create(String type,
                                       String frameSrc,
                                       String frameId,
                                       String frameName) {
    return new TaskAttributionTiming(type, frameSrc, frameId, frameName);
  }

  String frameSrc() const;
  String frameId() const;
  String frameName() const;

  DECLARE_VIRTUAL_TRACE();

  ~TaskAttributionTiming() override;

 private:
  TaskAttributionTiming(String type,
                        String frameSrc,
                        String frameId,
                        String frameName);

  String m_frameSrc;
  String m_frameId;
  String m_frameName;
};

}  // namespace blink

#endif  // TaskAttributionTiming_h
