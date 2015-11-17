// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/serial/serial.mojom.h"

namespace battor {

// A BattOrAgent is a class used to synchronously communicate with a BattOr for
// the purpose of collecting power samples. A BattOr is an external USB device
// that's capable of recording accurate, high-frequency (2000Hz) power samples.
//
// Because the communication is synchronous and passes over a serial connection,
// callers wishing to avoid blocking the thread (like the Chromium tracing
// controller) should issue these commands in a separate thread.
class BattOrAgent {
 public:
  enum BattOrError {
    BATTOR_ERROR_NONE,
  };

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
  std::string path_;

  DISALLOW_COPY_AND_ASSIGN(BattOrAgent);
};

}  // namespace battor
