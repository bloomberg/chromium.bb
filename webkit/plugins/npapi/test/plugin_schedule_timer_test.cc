// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/test/plugin_schedule_timer_test.h"

#include "base/logging.h"
#include "webkit/plugins/npapi/test/plugin_client.h"

using base::Time;

namespace NPAPIClient {

// The times below are accurate but they are not tested against because it
// might make the test flakey.
ScheduleTimerTest::Event
    ScheduleTimerTest::schedule_[ScheduleTimerTest::kNumEvents] = {
  {    0, -1,  0, 100, false, -1 },  // schedule 0 100ms no-repeat
  {  100,  0,  0, 200, false, -1 },  // schedule 0 200ms no-repeat
  {  300,  0,  0, 100,  true, -1 },  // schedule 0 100ms repeat
  {  400,  0,  1,  50,  true, -1 },  // schedule 1 50ms repeat
  {  450,  1, -1,   0,  true, -1 },  // receive 1 repeating
  {  500,  0, -1,   0,  true, -1 },  // receive 0 repeating
  {  500,  1, -1,   0,  true, -1 },  // receive 1 repeating
  {  550,  1, -1,   0,  true, -1 },  // receive 1 repeating
  {  600,  0, -1,   0,  true,  0 },  // receive 0 repeating and unschedule
  {  600,  1,  2, 400,  true,  1 },  // receive 1 repeating and unschedule
  { 1000,  2, -1,   0,  true,  2 },  // receive final and unschedule
};

ScheduleTimerTest::ScheduleTimerTest(
    NPP id, NPNetscapeFuncs *host_functions)
    : PluginTest(id, host_functions),
      num_received_events_(0) {
  for (int i = 0; i < kNumTimers; ++i) {
    timer_ids_[i] = 0;
  }
  for (int i = 0; i < kNumEvents; ++i) {
    received_events_[i] = false;
  }
}

NPError ScheduleTimerTest::New(
    uint16 mode, int16 argc, const char* argn[], const char* argv[],
    NPSavedData* saved) {
  NPError error = PluginTest::New(mode, argc, argn, argv, saved);
  if (error != NPERR_NO_ERROR)
    return error;

  start_time_ = Time::Now();
  HandleEventIndex(0);

  return NPERR_NO_ERROR;
}

void ScheduleTimerTest::OnTimer(uint32 timer_id) {
  Time current_time = Time::Now();
  int relative_time = static_cast<int>(
      (current_time - start_time_).InMilliseconds());

  // See if there is a matching unreceived event.
  int event_index = FindUnreceivedEvent(relative_time, timer_id);
  if (event_index < 0) {
    SetError("Received unexpected timer event");
    SignalTestCompleted();
    return;
  }

  HandleEventIndex(event_index);

  // Finish test if all events have happened.
  if (num_received_events_ == kNumEvents)
    SignalTestCompleted();
}

int ScheduleTimerTest::FindUnreceivedEvent(int time, uint32 timer_id) {
  for (int i = 0; i < kNumEvents; ++i) {
    const Event& event = schedule_[i];
    if (!received_events_[i] &&
        timer_ids_[event.received_index] == timer_id) {
      return i;
    }
  }
  return -1;
}

namespace {
void OnTimerHelper(NPP id, uint32 timer_id) {
  ScheduleTimerTest* plugin_object =
      static_cast<ScheduleTimerTest*>(id->pdata);
  if (plugin_object) {
    plugin_object->OnTimer(timer_id);
  }
}
}

void ScheduleTimerTest::HandleEventIndex(int event_index) {
  const Event& event = schedule_[event_index];

  // Mark event as received.
  DCHECK(!received_events_[event_index]);
  received_events_[event_index] = true;
  ++num_received_events_;

  // Unschedule timer if present.
  if (event.unscheduled_index >= 0) {
    HostFunctions()->unscheduletimer(
        id(), timer_ids_[event.unscheduled_index]);
  }

  // Schedule timer if present.
  if (event.scheduled_index >= 0) {
    timer_ids_[event.scheduled_index] = HostFunctions()->scheduletimer(
        id(), event.scheduled_interval, event.schedule_repeated, OnTimerHelper);
  }
}

}  // namespace NPAPIClient
