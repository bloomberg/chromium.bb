// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/devtools_util.h"

#include "base/bind.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/lazy_context_id.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace devtools_util {

namespace {

// Helper to inspect an ExtensionHost after it has been loaded.
void InspectExtensionHost(
    std::unique_ptr<LazyContextTaskQueue::ContextInfo> context_info) {
  if (context_info != nullptr)
    DevToolsWindow::OpenDevToolsWindow(context_info->web_contents);
}

}  // namespace

void InspectBackgroundPage(const Extension* extension, Profile* profile) {
  DCHECK(extension);
  ExtensionHost* host = ProcessManager::Get(profile)
                            ->GetBackgroundHostForExtension(extension->id());
  if (host) {
    InspectExtensionHost(
        std::make_unique<LazyContextTaskQueue::ContextInfo>(host));
  } else {
    const LazyContextId context_id(profile, extension->id());
    context_id.GetTaskQueue()->AddPendingTask(
        context_id, base::BindOnce(&InspectExtensionHost));
  }
}

}  // namespace devtools_util
}  // namespace extensions
