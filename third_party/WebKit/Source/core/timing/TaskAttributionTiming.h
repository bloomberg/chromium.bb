// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskAttributionTiming_h
#define TaskAttributionTiming_h

#include "core/timing/PerformanceEntry.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class TaskAttributionTiming final : public PerformanceEntry {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Used when the LongTaskV2 flag is enabled.
  static TaskAttributionTiming* Create(String type,
                                       String container_type,
                                       String container_src,
                                       String container_id,
                                       String container_name,
                                       double start_time,
                                       double finish_time,
                                       String script_url) {
    return new TaskAttributionTiming(type, container_type, container_src,
                                     container_id, container_name, start_time,
                                     finish_time, script_url);
  }

  // Used when the LongTaskV2 flag is disabled.
  static TaskAttributionTiming* Create(String type,
                                       String container_type,
                                       String container_src,
                                       String container_id,
                                       String container_name) {
    return new TaskAttributionTiming(type, container_type, container_src,
                                     container_id, container_name, 0.0, 0.0,
                                     g_empty_string);
  }
  String containerType() const;
  String containerSrc() const;
  String containerId() const;
  String containerName() const;
  String scriptURL() const;

  DECLARE_VIRTUAL_TRACE();

  ~TaskAttributionTiming() override;

 private:
  TaskAttributionTiming(String type,
                        String container_type,
                        String container_src,
                        String container_id,
                        String container_name,
                        double start_time,
                        double finish_time,
                        String script_url);
  String container_type_;
  String container_src_;
  String container_id_;
  String container_name_;
  String script_url_;
};

}  // namespace blink

#endif  // TaskAttributionTiming_h
