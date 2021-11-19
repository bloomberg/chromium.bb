// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_EMBEDDER_EMBEDDER_H_
#define MOJO_CORE_EMBEDDER_EMBEDDER_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/core/embedder/configuration.h"

namespace mojo {
namespace core {

// Basic configuration/initialization ------------------------------------------

// Must be called first, or just after setting configuration parameters, to
// initialize the (global, singleton) system state. There is no corresponding
// shutdown operation: once the embedder is initialized, public Mojo C API calls
// remain available for the remainder of the process's lifetime.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER)
void Init(const Configuration& configuration);

// Like above but uses a default Configuration.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) void Init();

// Initialialization/shutdown for interprocess communication (IPC) -------------

// Retrieves the SequencedTaskRunner used for IPC I/O, as set by
// ScopedIPCSupport.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER)
scoped_refptr<base::SingleThreadTaskRunner> GetIOTaskRunner();

// InitFeatures will be called as soon as the base::FeatureList is initialized.
// NOTE: This is temporarily necessary because of how Mojo is started with
// respect to base::FeatureList.
//
// TODO(rockot): Remove once a long term solution is in place for using
// base::Features inside of Mojo.
COMPONENT_EXPORT(MOJO_CORE_EMBEDDER) void InitFeatures();

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_EMBEDDER_EMBEDDER_H_
