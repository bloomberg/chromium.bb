// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_

#include "base/macros.h"

namespace performance_manager {

class Graph;

// A process node follows the lifetime of a RenderProcessHost.
// It may reference zero or one processes at a time, but during its lifetime, it
// may reference more than one process. This can happen if the associated
// renderer crashes, and an associated frame is then reloaded or re-navigated.
// The state of the process node goes through:
// 1. Created, no PID.
// 2. Process started, have PID - in the case where the associated render
//    process fails to start, this state may not occur.
// 3. Process died or failed to start, have exit status.
// 4. Back to 2.
class ProcessNode {
 public:
  ProcessNode();
  virtual ~ProcessNode();

  // Returns the graph to which this node belongs.
  virtual Graph* GetGraph() const = 0;

  // Returns the private key which is used for indexing this object in the
  // graph. This is an opaque pointer strictly used for implementation.
  virtual const void* GetIndexingKey() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessNode);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_GRAPH_PROCESS_NODE_H_
