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

void BattOrAgent::StartTracing(const SendCallback& callback) {
  // TODO(charliea): Tell the BattOr to start tracing.
  callback.Run(device::serial::SEND_ERROR_NONE);
}

void BattOrAgent::StopTracing(std::string* trace_output,
                              const SendReceiveCallback& callback) {
  // TODO(charliea): Tell the BattOr to stop tracing.
  *trace_output = "battor trace output";
  callback.Run(device::serial::SEND_ERROR_NONE,
               device::serial::RECEIVE_ERROR_NONE);
}

void BattOrAgent::RecordClockSyncMarker(const std::string& marker,
                                        const SendReceiveCallback& callback) {
  // TODO(charliea): Tell the BattOr to record the specified clock sync marker.
  callback.Run(device::serial::SEND_ERROR_NONE,
               device::serial::RECEIVE_ERROR_NONE);
}

void BattOrAgent::IssueClockSyncMarker(const SendCallback& callback) {
  // TODO(charliea): Tell atrace to issue a clock sync marker.
  callback.Run(device::serial::SEND_ERROR_NONE);
}

}  // namespace battor
