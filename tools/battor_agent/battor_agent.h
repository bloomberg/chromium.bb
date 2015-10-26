// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"

namespace battor {

class BattOrAgent {
 public:
  explicit BattOrAgent(const std::string& path);
  virtual ~BattOrAgent();

  // Tells the BattOr to start tracing.
  void StartTracing();

  // Tells the BattOr to stop tracing and returns the trace contents.
  void StopTracing(std::string* out_trace);

  // Tells the BattOr to record a clock sync marker in its own trace log.
  void RecordClockSyncMarker(const std::string& marker);

  // Tells the BattOr to issue clock sync markers to all other tracing
  // agents that it's connected to.
  void IssueClockSyncMarker();

  // Returns whether the BattOr is able to record clock sync markers
  // in its own trace log.
  static bool SupportsExplicitClockSync() { return true; }

 private:
  std::string path_;

  DISALLOW_COPY_AND_ASSIGN(BattOrAgent);
};

}  // namespace battor
