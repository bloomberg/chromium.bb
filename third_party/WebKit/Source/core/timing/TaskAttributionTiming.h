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
                                       String containerType,
                                       String containerSrc,
                                       String containerId,
                                       String containerName) {
    return new TaskAttributionTiming(type, containerType, containerSrc,
                                     containerId, containerName);
  }

  String containerType() const;
  String containerSrc() const;
  String containerId() const;
  String containerName() const;

  DECLARE_VIRTUAL_TRACE();

  ~TaskAttributionTiming() override;

 private:
  TaskAttributionTiming(String type,
                        String containerType,
                        String containerSrc,
                        String containerId,
                        String containerName);

  String m_containerType;
  String m_containerSrc;
  String m_containerId;
  String m_containerName;
};

}  // namespace blink

#endif  // TaskAttributionTiming_h
