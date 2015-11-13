// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

#include <iostream>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

using device::serial::ReceiveError;
using device::serial::SendError;

namespace battor {

BattOrAgent::BattOrAgent(const std::string& path) : path_(path) {
  // TODO(charliea): Open up a serial connection with the BattOr.
}

BattOrAgent::~BattOrAgent() {
  // TODO(charliea): Close the serial connection with the BattOr.
}

BattOrAgent::BattOrError BattOrAgent::StartTracing() {
  // TODO(charliea): Tell the BattOr to start tracing.
  return BATTOR_ERROR_NONE;
}

BattOrAgent::BattOrError BattOrAgent::StopTracing(std::string* trace_output) {
  // TODO(charliea): Tell the BattOr to stop tracing.
  *trace_output = "battor trace output";
  return BATTOR_ERROR_NONE;
}

BattOrAgent::BattOrError BattOrAgent::RecordClockSyncMarker(
    const std::string& marker) {
  // TODO(charliea): Tell the BattOr to record the specified clock sync marker.
  return BATTOR_ERROR_NONE;
}

BattOrAgent::BattOrError BattOrAgent::IssueClockSyncMarker() {
  // TODO(charliea): Tell atrace to issue a clock sync marker.
  return BATTOR_ERROR_NONE;
}

}  // namespace battor
