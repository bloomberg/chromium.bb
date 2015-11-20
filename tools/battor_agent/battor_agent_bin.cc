// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a thin binary wrapper around the BattOr Agent
// library. This binary wrapper provides a means for non-C++ tracing
// controllers, such as Telemetry and Android Systrace, to issue high-level
// tracing commands to the BattOr..

#include <iostream>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "device/serial/serial.mojom.h"
#include "tools/battor_agent/battor_agent.h"
#include "tools/battor_agent/battor_error.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

using device::serial::ReceiveError;
using device::serial::SendError;

namespace {

void PrintUsage() {
  cerr << "Usage: battor_agent <command> <arguments>" << endl
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

// Checks if an error occurred and, if it did, prints the error and exits
// with an error code.
void CheckError(battor::BattOrError error) {
  if (error != battor::BATTOR_ERROR_NONE) {
    cerr << "Error when communicating with the BattOr: " << error << endl;
    exit(1);
  }
}

void StartTracing(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  battor::BattOrAgent agent(path);

  CheckError(agent.StartTracing());
}

void StopTracing(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  string trace_output;
  battor::BattOrAgent agent(path);

  CheckError(agent.StopTracing(&trace_output));
}

void SupportsExplicitClockSync() {
  cout << battor::BattOrAgent::SupportsExplicitClockSync() << endl;
}

void RecordClockSyncMarker(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  string marker = GetArg(3, argc, argv);
  battor::BattOrAgent agent(path);

  // TODO(charliea): Write time to STDOUT.
  CheckError(agent.RecordClockSyncMarker(marker));
  // TODO(charliea): Write time to STDOUT.
}

void IssueClockSyncMarker(int argc, char* argv[]) {
  string path = GetArg(2, argc, argv);
  battor::BattOrAgent agent(path);

  CheckError(agent.IssueClockSyncMarker());
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
