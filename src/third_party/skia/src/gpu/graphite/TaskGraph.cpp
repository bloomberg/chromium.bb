/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/TaskGraph.h"

namespace skgpu::graphite {

TaskGraph::TaskGraph() {}
TaskGraph::~TaskGraph() {}

void TaskGraph::add(sk_sp<Task> task) {
    fTasks.emplace_back(std::move(task));
}

bool TaskGraph::prepareResources(ResourceProvider* resourceProvider) {
    for (const auto& task: fTasks) {
        if (!task->prepareResources(resourceProvider)) {
            return false;
        }
    }

    return true;
}

bool TaskGraph::addCommands(ResourceProvider* resourceProvider, CommandBuffer* commandBuffer) {
    for (const auto& task: fTasks) {
        if (!task->addCommands(resourceProvider, commandBuffer)) {
            return false;
        }
    }

    return true;
}

void TaskGraph::reset() {
    fTasks.clear();
}

} // namespace skgpu::graphite
