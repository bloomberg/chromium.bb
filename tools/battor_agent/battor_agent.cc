// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/battor_agent/battor_agent.h"

namespace battor {

BattOrAgent::BattOrAgent(const std::string& path) : path_(path) {}
BattOrAgent::~BattOrAgent() {}

BattOrError BattOrAgent::StartTracing() {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell the BattOr to start tracing.
  return BATTOR_ERROR_NONE;
}

BattOrError BattOrAgent::StopTracing(std::string* trace_output) {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell the BattOr to stop tracing.
  *trace_output = "battor trace output";

  ResetConnection();
  return BATTOR_ERROR_NONE;
}

BattOrError BattOrAgent::RecordClockSyncMarker(const std::string& marker) {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell the BattOr to record the specified clock sync marker.
  return BATTOR_ERROR_NONE;
}

BattOrError BattOrAgent::IssueClockSyncMarker() {
  BattOrError error = ConnectIfNeeded();
  if (error != BATTOR_ERROR_NONE)
    return error;

  // TODO(charliea): Tell atrace to issue a clock sync marker.
  return BATTOR_ERROR_NONE;
}

void BattOrAgent::ResetConnection() {
  connection_ = nullptr;
}

BattOrError BattOrAgent::ConnectIfNeeded() {
  if (connection_)
    return BATTOR_ERROR_NONE;

  connection_ = BattOrConnection::Create(path_);

  return connection_ ? BATTOR_ERROR_NONE : BATTOR_ERROR_CONNECTION_FAILED;
}

}  // namespace battor
