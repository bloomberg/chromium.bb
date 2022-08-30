/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_TaskGraph_DEFINED
#define skgpu_graphite_TaskGraph_DEFINED

#include <vector>
#include "src/gpu/graphite/Task.h"

namespace skgpu::graphite {
class CommandBuffer;
class ResourceProvider;

class TaskGraph {
public:
    TaskGraph();
    ~TaskGraph();

    void add(sk_sp<Task>);

    // Returns true on success; false on failure
    bool prepareResources(ResourceProvider*);
    bool addCommands(ResourceProvider*, CommandBuffer*);

    void reset();

protected:
private:
    std::vector<sk_sp<Task>> fTasks;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_TaskGraph_DEFINED
