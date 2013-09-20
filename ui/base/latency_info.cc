// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/latency_info.h"

#include <algorithm>

namespace ui {

LatencyInfo::LatencyInfo() {
}

LatencyInfo::~LatencyInfo() {
}

void LatencyInfo::MergeWith(const LatencyInfo& other) {
  for (LatencyMap::const_iterator it = other.latency_components.begin();
       it != other.latency_components.end();
       ++it) {
    AddLatencyNumberWithTimestamp(it->first.first,
                                  it->first.second,
                                  it->second.sequence_number,
                                  it->second.event_time,
                                  it->second.event_count);
  }
}

void LatencyInfo::AddNewLatencyFrom(const LatencyInfo& other) {
    for (LatencyMap::const_iterator it = other.latency_components.begin();
         it != other.latency_components.end();
         ++it) {
      if (!FindLatency(it->first.first, it->first.second, NULL)) {
        AddLatencyNumberWithTimestamp(it->first.first,
                                      it->first.second,
                                      it->second.sequence_number,
                                      it->second.event_time,
                                      it->second.event_count);
      }
    }
}

void LatencyInfo::AddLatencyNumber(LatencyComponentType component,
                                   int64 id,
                                   int64 component_sequence_number) {
  AddLatencyNumberWithTimestamp(component, id, component_sequence_number,
                                base::TimeTicks::HighResNow(), 1);
}

void LatencyInfo::AddLatencyNumberWithTimestamp(LatencyComponentType component,
                                                int64 id,
                                                int64 component_sequence_number,
                                                base::TimeTicks time,
                                                uint32 event_count) {
  LatencyMap::key_type key = std::make_pair(component, id);
  LatencyMap::iterator it = latency_components.find(key);
  if (it == latency_components.end()) {
    LatencyComponent info = {component_sequence_number, time, event_count};
    latency_components[key] = info;
    return;
  }
  it->second.sequence_number = std::max(component_sequence_number,
                                        it->second.sequence_number);
  uint32 new_count = event_count + it->second.event_count;
  if (event_count > 0 && new_count != 0) {
    // Do a weighted average, so that the new event_time is the average of
    // the times of events currently in this structure with the time passed
    // into this method.
    it->second.event_time += (time - it->second.event_time) * event_count /
        new_count;
    it->second.event_count = new_count;
  }
}

bool LatencyInfo::FindLatency(LatencyComponentType type,
                              int64 id,
                              LatencyComponent* output) const {
  LatencyMap::const_iterator it = latency_components.find(
      std::make_pair(type, id));
  if (it == latency_components.end())
    return false;
  if (output)
    *output = it->second;
  return true;
}

void LatencyInfo::Clear() {
  latency_components.clear();
}

}  // namespace ui
