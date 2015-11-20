// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BATTOR_AGENT_BATTOR_AGENT_H_
#define TOOLS_BATTOR_AGENT_BATTOR_AGENT_H_

#include <string>

#include "base/macros.h"
#include "tools/battor_agent/battor_connection.h"
#include "tools/battor_agent/battor_error.h"

namespace battor {

// A BattOrAgent is a class used to synchronously communicate with a BattOr for
// the purpose of collecting power samples. A BattOr is an external USB device
// that's capable of recording accurate, high-frequency (2000Hz) power samples.
//
// Because the communication is synchronous and passes over a serial connection,
// callers wishing to avoid blocking the thread (like the Chromium tracing
// controller) should issue these commands in a separate thread.
//
// The serial connection is automatically opened when the first command
// (e.g. StartTracing(), StopTracing(), etc.) is issued, and automatically
// closed when either StopTracing() or the destructor is called. For Telemetry,
// this means that the connection must be reinitialized for every command that's
// issued because a new BattOrAgent is constructed. For Chromium, we use the
// same BattOrAgent for multiple commands and thus avoid having to reinitialize
// the serial connection.
//
// This class is NOT thread safe.
class BattOrAgent {
 public:
  explicit BattOrAgent(const std::string& path);
  virtual ~BattOrAgent();

  // Tells the BattOr to start tracing.
  BattOrError StartTracing();

  // Tells the BattOr to stop tracing and write the trace output to the
  // specified string.
  BattOrError StopTracing(std::string* trace_output);

  // Tells the BattOr to record a clock sync marker in its own trace log.
  BattOrError RecordClockSyncMarker(const std::string& marker);

  // Tells the BattOr to issue clock sync markers to all other tracing agents
  // to which it's connected.
  BattOrError IssueClockSyncMarker();

  // Returns whether the BattOr is able to record clock sync markers in its own
  // trace log.
  static bool SupportsExplicitClockSync() { return true; }

 private:
  // Initializes the serial connection with the BattOr. If the connection
  // already exists, BATTOR_ERROR_NONE is immediately returned.
  BattOrError ConnectIfNeeded();

  // Resets the connection to its unopened state.
  void ResetConnection();

  // The path of the BattOr (e.g. "/dev/tty.battor_serial").
  std::string path_;

  // Serial connection the BattOr.
  scoped_ptr<BattOrConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(BattOrAgent);
};

}  // namespace battor

#endif  // TOOLS_BATTOR_AGENT_BATTOR_AGENT_H_
