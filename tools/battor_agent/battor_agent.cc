// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

#include <iostream>

namespace battor {

BattOrAgent::BattOrAgent(const std::string& path) : path_(path) {
  // TODO: Open up a serial connection with the BattOr.
}

BattOrAgent::~BattOrAgent() {
  // TODO: Close the serial connection with the BattOr.
}

void BattOrAgent::StartTracing() {
  // TODO: Tell the BattOr to start tracing.
}

void BattOrAgent::StopTracing(std::string* out_trace) {
  // TODO: Tell the BattOr to stop tracing.
}

void BattOrAgent::RecordClockSyncMarker(const std::string& marker) {
  // TODO: Tell the BattOr to record the specified clock sync marker.
}

void BattOrAgent::IssueClockSyncMarker() {
  // TODO: Tell atrace to issue a clock sync marker.
}

}  // namespace battor
