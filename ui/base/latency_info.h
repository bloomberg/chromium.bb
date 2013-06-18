// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_LATENCY_INFO_H_
#define UI_BASE_LATENCY_INFO_H_

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/time.h"
#include "ui/base/ui_export.h"

namespace ui {

enum LatencyComponentType {
  // Timestamp when the input event is sent from RenderWidgetHost to renderer.
  INPUT_EVENT_LATENCY_RWH_COMPONENT,
  // Original timestamp for input event (e.g. timestamp from kernel).
  INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT,
  // Timestamp when the UI event is created.
  INPUT_EVENT_LATENCY_UI_COMPONENT,
  // Timestamp when the event is acked from renderer.
  INPUT_EVENT_LATENCY_ACKED_COMPONENT
};

struct UI_EXPORT LatencyInfo {
  struct LatencyComponent {
    // Nondecreasing number that can be used to determine what events happened
    // in the component at the time this struct was sent on to the next
    // component.
    int64 sequence_number;
    // Average time of events that happened in this component.
    base::TimeTicks event_time;
    // Count of events that happened in this component
    uint32 event_count;
  };

  // Map a Latency Component (with a component-specific int64 id) to a
  // component info.
  typedef std::map<std::pair<LatencyComponentType, int64>, LatencyComponent>
      LatencyMap;

  LatencyInfo();

  ~LatencyInfo();

  // Merges the contents of another LatencyInfo into this one.
  void MergeWith(const LatencyInfo& other);

  // Modifies the current sequence number for a component, and adds a new
  // sequence number with the current timestamp.
  void AddLatencyNumber(LatencyComponentType component,
                        int64 id,
                        int64 component_sequence_number);

  // Modifies the current sequence number and adds a certain number of events
  // for a specific component.
  void AddLatencyNumberWithTimestamp(LatencyComponentType component,
                                     int64 id,
                                     int64 component_sequence_number,
                                     base::TimeTicks time,
                                     uint32 event_count);

  void Clear();

  LatencyMap latency_components;

  // This represents the final time that a frame is displayed it.
  base::TimeTicks swap_timestamp;
};

}  // namespace ui

#endif  // UI_BASE_LATENCY_INFO_H_
