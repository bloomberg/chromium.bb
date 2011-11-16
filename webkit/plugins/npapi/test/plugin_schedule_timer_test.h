// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_SCHEDULE_TIMER_TEST_H
#define WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_SCHEDULE_TIMER_TEST_H

#include "base/compiler_specific.h"
#include "base/time.h"
#include "webkit/plugins/npapi/test/plugin_test.h"

namespace NPAPIClient {

// This class tests scheduling and unscheduling of timers using
// NPN_ScheduleTimer and NPN_UnscheduleTimer.
class ScheduleTimerTest : public PluginTest {
 public:
  ScheduleTimerTest(NPP id, NPNetscapeFuncs *host_functions);

  virtual NPError New(uint16 mode, int16 argc, const char* argn[],
                      const char* argv[], NPSavedData* saved) OVERRIDE;

  void OnTimer(uint32 timer_id);

 private:
  // Table mapping timer index (as used in event schedule) to timer id.
  static const int kNumTimers = 3;
  uint32 timer_ids_[kNumTimers];

  // Schedule of events for test.
  static const int kNumEvents = 11;
  struct Event {
    int time;

    // The index of the timer that triggered the event or -1 for the first
    // event.
    int received_index;

    // The index of the timer to schedule on this event or -1.
    int scheduled_index;

    // Info about the timer to be scheduled (if any).
    uint32 scheduled_interval;
    bool schedule_repeated;

    // The index of the timer to unschedule on this event or -1.
    int unscheduled_index;
  };
  static Event schedule_[kNumEvents];
  int num_received_events_;

  // Set of events that have been received (by index).
  bool received_events_[kNumEvents];

  // Time of initial event.
  base::Time start_time_;

  // Returns index of matching unreceived event or -1 if not found.
  int FindUnreceivedEvent(int time, uint32 timer_id);
  void HandleEventIndex(int event_index);
};

}  // namespace NPAPIClient

#endif  // WEBKIT_PLUGINS_NPAPI_TEST_PLUGIN_SCHEDULE_TIMER_TEST_H
