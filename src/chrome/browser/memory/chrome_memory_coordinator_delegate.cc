// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/chrome_memory_coordinator_delegate.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/resource_coordinator/tab_manager.h"
#endif

namespace memory {

// static
std::unique_ptr<content::MemoryCoordinatorDelegate>
ChromeMemoryCoordinatorDelegate::Create() {
  return base::WrapUnique(new ChromeMemoryCoordinatorDelegate);
}

ChromeMemoryCoordinatorDelegate::ChromeMemoryCoordinatorDelegate() {}

ChromeMemoryCoordinatorDelegate::~ChromeMemoryCoordinatorDelegate() {}

void ChromeMemoryCoordinatorDelegate::DiscardTab(bool skip_unload_handlers) {
#if !defined(OS_ANDROID)
  if (g_browser_process->GetTabManager()) {
    g_browser_process->GetTabManager()->DiscardTab(
        skip_unload_handlers ? mojom::LifecycleUnitDiscardReason::URGENT
                             : mojom::LifecycleUnitDiscardReason::PROACTIVE);
  }
#endif
}

}  // namespace memory
