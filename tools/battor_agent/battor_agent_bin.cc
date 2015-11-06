// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a thin binary wrapper around the BattOr Agent
// library. This binary wrapper provides a means for non-C++ tracing
// controllers, such as Telemetry and Android Systrace, to issue high-level
// tracing commands to the BattOr..

#include <iostream>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "device/serial/serial.mojom.h"
#include "tools/battor_agent/battor_agent.h"

using std::cout;
using std::endl;
using std::string;

using device::serial::ReceiveError;
using device::serial::SendError;

namespace {

// The maximum amount of time to wait for the BattOr Agent to execute a command.
static const base::TimeDelta COMMAND_TIMEOUT = base::TimeDelta::FromSeconds(10);

void PrintUsage() {
  cout << "Usage: battor_agent <command> <arguments>" << endl
       << endl
       << "Commands:" << endl
       << endl
       << "  StartTracing <path>" << endl
       << "  StopTracing <path>" << endl
       << "  SupportsExplicitClockSync" << endl
       << "  RecordClockSyncMarker <path> <marker>" << endl
       << "  IssueClockSyncMarker <path>" << endl
       << "  Help" << endl;
}

// Retrieves argument argnum from the argument list, printing the usage
// guidelines and exiting with an error code if the argument doesn't exist.
string GetArg(int argnum, int argc, char* argv[]) {
  if (argnum >= argc) {
    PrintUsage();
    exit(1);
  }

  return argv[argnum];
}

// Checks if a send error occurred and, if it did, prints the error and exits
// with an error code.
void CheckSendError(SendError error) {
  if (error != device::serial::SEND_ERROR_NONE) {
    printf("Error occured while sending: %d", error);
    exit(1);
  }
}

// Checks if a receive error occurred and, if it did, prints the error and exits
// with an error code.
void CheckReceiveError(ReceiveError error) {
  if (error != device::serial::RECEIVE_ERROR_NONE) {
    cout << "Error occured while receiving: " << error << endl;
    exit(1);
  }
}

void OnStartTracingComplete(base::WaitableEvent* complete_event,
                            SendError error) {
  CheckSendError(error);
  cout << "Tracing successfully started." << endl;
  complete_event->Signal();
}

void OnStopTracingComplete(string* trace_output,
                           base::WaitableEvent* complete_event,
                           SendError send_error,
                           ReceiveError receive_error) {
  CheckSendError(send_error);
  CheckReceiveError(receive_error);
  cout << *trace_output << endl;
  complete_event->Signal();
}

void OnRecordClockSyncMarkerComplete(base::WaitableEvent* complete_event,
                                     SendError send_error,
                                     ReceiveError receive_error) {
  CheckSendError(send_error);
  CheckReceiveError(receive_error);
  // TODO(charliea): Write time to STDOUT.
  cout << "Successfully recorded clock sync marker." << endl;
  complete_event->Signal();
}

void OnIssueClockSyncMarkerComplete(base::WaitableEvent* complete_event,
                                    SendError error) {
  CheckSendError(error);
  cout << "Successfully issued clock sync marker." << endl;
  complete_event->Signal();
}

void StartTracing(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  battor::BattOrAgent agent(path);
  base::WaitableEvent command_complete_event(false, false);

  agent.StartTracing(
      base::Bind(&OnStartTracingComplete, &command_complete_event));
  command_complete_event.TimedWait(COMMAND_TIMEOUT);
}

void StopTracing(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  string trace_output;
  battor::BattOrAgent agent(path);
  base::WaitableEvent command_complete_event(false, false);

  agent.StopTracing(&trace_output,
                    base::Bind(&OnStopTracingComplete, &trace_output,
                               &command_complete_event));
  command_complete_event.TimedWait(COMMAND_TIMEOUT);
}

void SupportsExplicitClockSync() {
  cout << battor::BattOrAgent::SupportsExplicitClockSync() << endl;
}

void RecordClockSyncMarker(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  string marker = GetArg(3, argc, argv);
  base::WaitableEvent command_complete_event(false, false);
  battor::BattOrAgent agent(path);

  // TODO(charliea): Write time to STDOUT.
  agent.RecordClockSyncMarker(
      marker,
      base::Bind(&OnRecordClockSyncMarkerComplete, &command_complete_event));
  command_complete_event.TimedWait(COMMAND_TIMEOUT);
}

void IssueClockSyncMarker(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  base::WaitableEvent command_complete_event(false, false);
  battor::BattOrAgent agent(path);

  agent.IssueClockSyncMarker(
      base::Bind(&OnIssueClockSyncMarkerComplete, &command_complete_event));
  command_complete_event.TimedWait(COMMAND_TIMEOUT);
}

}  // namespace

int main(int argc, char* argv[]) {
  string cmd = GetArg(1, argc, argv);

  if (cmd == "StartTracing") {
    StartTracing(argc, argv);
  } else if (cmd == "StopTracing") {
    StopTracing(argc, argv);
  } else if (cmd == "SupportsExplicitClockSync") {
    SupportsExplicitClockSync();
  } else if (cmd == "RecordClockSyncMarker") {
    RecordClockSyncMarker(argc, argv);
  } else if (cmd == "IssueClockSyncMarker") {
    IssueClockSyncMarker(argc, argv);
  } else {
    PrintUsage();
    return 1;
  }

  return 0;
}
