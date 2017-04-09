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
  static TaskAttributionTiming* Create(String type,
                                       String container_type,
                                       String container_src,
                                       String container_id,
                                       String container_name) {
    return new TaskAttributionTiming(type, container_type, container_src,
                                     container_id, container_name);
  }

  String containerType() const;
  String containerSrc() const;
  String containerId() const;
  String containerName() const;

  DECLARE_VIRTUAL_TRACE();

  ~TaskAttributionTiming() override;

 private:
  TaskAttributionTiming(String type,
                        String container_type,
                        String container_src,
                        String container_id,
                        String container_name);

  String container_type_;
  String container_src_;
  String container_id_;
  String container_name_;
};

}  // namespace blink

#endif  // TaskAttributionTiming_h
