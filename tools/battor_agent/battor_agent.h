// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "device/serial/serial.mojom.h"

namespace battor {

class BattOrAgent {
 public:
  typedef base::Callback<void(device::serial::SendError)> SendCallback;
  typedef base::Callback<void(device::serial::SendError,
                              device::serial::ReceiveError)>
      SendReceiveCallback;

  explicit BattOrAgent(const std::string& path);
  virtual ~BattOrAgent();

  // Tells the BattOr to start tracing and calls the callback when complete.
  void StartTracing(const SendCallback& callback);

  // Tells the BattOr to stop tracing and write the trace output to the
  // specified string and calls the callback when complete.
  void StopTracing(std::string* trace_output,
                   const SendReceiveCallback& callback);

  // Tells the BattOr to record a clock sync marker in its own trace log and
  // calls the callback when complete.
  void RecordClockSyncMarker(const std::string& marker,
                             const SendReceiveCallback& callback);

  // Tells the BattOr to issue clock sync markers to all other tracing agents
  // that it's connected to and calls the callback when complete.
  void IssueClockSyncMarker(const SendCallback& callback);

  // Returns whether the BattOr is able to record clock sync markers in its own
  // trace log.
  static bool SupportsExplicitClockSync() { return true; }

 private:
  std::string path_;

  DISALLOW_COPY_AND_ASSIGN(BattOrAgent);
};

}  // namespace battor
