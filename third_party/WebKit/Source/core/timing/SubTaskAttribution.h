// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SubTaskAttribution_h
#define SubTaskAttribution_h

#include "core/dom/DOMHighResTimeStamp.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class SubTaskAttribution {
 public:
  using EntriesVector = Vector<std::unique_ptr<SubTaskAttribution>>;

  static std::unique_ptr<SubTaskAttribution> Create(String sub_task_name,
                                                    String script_url,
                                                    double start_time,
                                                    double duration) {
    return WTF::MakeUnique<SubTaskAttribution>(sub_task_name, script_url,
                                               start_time, duration);
  }
  SubTaskAttribution(String sub_task_name,
                     String script_url,
                     double start_time,
                     double duration);
  inline String subTaskName() const { return sub_task_name_; }
  inline String scriptURL() const { return script_url_; }
  inline double startTime() const { return start_time_; }
  inline double duration() const { return duration_; }

  inline DOMHighResTimeStamp highResStartTime() const {
    return high_res_start_time_;
  }
  inline DOMHighResTimeStamp highResDuration() const {
    return high_res_duration_;
  }
  void setHighResStartTime(DOMHighResTimeStamp high_res_start_time) {
    high_res_start_time_ = high_res_start_time;
  }
  void setHighResDuration(DOMHighResTimeStamp high_res_duration) {
    high_res_duration_ = high_res_duration;
  }

 private:
  String sub_task_name_;
  String script_url_;
  double start_time_;
  double duration_;
  DOMHighResTimeStamp high_res_start_time_;
  DOMHighResTimeStamp high_res_duration_;
};

}  // namespace blink

#endif  // SubTaskAttribution_h
