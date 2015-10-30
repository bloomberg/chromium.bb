// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a thin binary wrapper around the BattOr Agent
// library. This binary wrapper provides a means for non-C++ tracing
// controllers, such as Telemetry and Android Systrace, to issue high-level
// tracing commands to the BattOr..

#include <iostream>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "tools/battor_agent/battor_agent.h"

using std::cout;
using std::endl;
using std::string;

namespace {

// An event used to signal that the BattOr Agent has finished executing its
// command.
base::WaitableEvent g_stop_tracing_complete_event(false, false);

void PrintUsage() {
  cout << "Usage: battor_agent <command> <arguments>" << endl << endl
       << "Commands:" << endl << endl
       << "  StartTracing <path>" << endl
       << "  StopTracing <path>" << endl
       << "  SupportsExplicitClockSync" << endl
       << "  RecordClockSyncMarker <path> <marker>" << endl
       << "  IssueClockSyncMarker <path>" << endl
       << "  Help" << endl;
}

// Retrieves argument argnum from the argument list and stores it into value,
// returning whether the operation was successful and printing the usage
// guidelines if it wasn't.
bool GetArg(int argnum, int argc, char* argv[], string* value) {
  if (argnum >= argc) {
    PrintUsage();
    return false;
  }

  *value = argv[argnum];
  return true;
}

void OnCommandComplete() {
  g_stop_tracing_complete_event.Signal();
}

} // namespace

int main(int argc, char* argv[]) {
  string cmd;
  if (!GetArg(1, argc, argv, &cmd))
    return 1;

  if (cmd == "StartTracing") {
    string path;
    if (!GetArg(2, argc, argv, &path))
      return 1;

    cout << "Calling StartTracing()" << endl;
    battor::BattOrAgent(path).StartTracing();
  } else if (cmd == "StopTracing") {
    string path;
    if (!GetArg(2, argc, argv, &path))
      return 1;

    cout << "Calling StopTracing()" << endl;
    std::string trace_output;
    battor::BattOrAgent(path)
        .StopTracing(&trace_output, base::Bind(&OnCommandComplete));
    g_stop_tracing_complete_event.Wait();
    cout << trace_output << endl;
  } else if (cmd == "SupportsExplicitClockSync") {
    cout << "Calling SupportsExplicitClockSync" << endl;
    cout << battor::BattOrAgent::SupportsExplicitClockSync() << endl;
  } else if (cmd == "RecordClockSyncMarker") {
    string path, marker;
    if (!GetArg(2, argc, argv, &path) || !GetArg(3, argc, argv, &marker))
      return 1;

    cout << "Marker: " << marker << endl;
    cout << "Calling RecordClockSyncMarker()" << endl;
    // TODO(charliea): Write the  time to STDOUT
    battor::BattOrAgent(path)
        .RecordClockSyncMarker(marker, base::Bind(&OnCommandComplete));
    g_stop_tracing_complete_event.Wait();
    // TODO(charliea): Write the time to STDOUT
  } else if (cmd == "IssueClockSyncMarker") {
    string path;
    if (!GetArg(2, argc, argv, &path))
      return 1;

    cout << "Calling IssueClockSyncMarker" << endl;
    battor::BattOrAgent(path).IssueClockSyncMarker();
  } else {
    PrintUsage();
    return 1;
  }

  return 0;
}
