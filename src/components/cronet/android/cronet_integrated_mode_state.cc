// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/cronet_integrated_mode_state.h"

#include "base/atomicops.h"

namespace cronet {
namespace {

base::subtle::AtomicWord g_integrated_mode_network_task_runner = 0;

}  // namespace

void SetIntegratedModeNetworkTaskRunner(
    base::SingleThreadTaskRunner* network_task_runner) {
  CHECK_EQ(base::subtle::Acquire_CompareAndSwap(
               &g_integrated_mode_network_task_runner, 0,
               reinterpret_cast<base::subtle::AtomicWord>(network_task_runner)),
           0);
}

base::SingleThreadTaskRunner* GetIntegratedModeNetworkTaskRunner() {
  base::subtle::AtomicWord task_runner =
      base::subtle::Release_Load(&g_integrated_mode_network_task_runner);
  CHECK(task_runner);
  return reinterpret_cast<base::SingleThreadTaskRunner*>(task_runner);
}

}  // namespace cronet
