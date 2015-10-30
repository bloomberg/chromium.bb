// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace battor {

class BattOrAgent {
 public:
  explicit BattOrAgent(const std::string& path);
  virtual ~BattOrAgent();

  // Tells the BattOr (using a best-effort signal) to start tracing.
  void StartTracing();

  // Tells the BattOr to stop tracing and write the trace output to
  // the specified location and calls the callback when complete.
  void StopTracing(std::string* trace_output, const base::Closure& callback);

  // Tells the BattOr to record a clock sync marker in its own trace
  // log and calls the callback when complete.
  void RecordClockSyncMarker(const std::string& marker,
                             const base::Closure& callback);

  // Tells the BattOr (using a best-effort signal) to issue clock sync
  // markers to all other tracing agents that it's connected to.
  void IssueClockSyncMarker();

  // Returns whether the BattOr is able to record clock sync markers
  // in its own trace log.
  static bool SupportsExplicitClockSync() { return true; }

 private:
  std::string path_;

  DISALLOW_COPY_AND_ASSIGN(BattOrAgent);
};

}  // namespace battor
