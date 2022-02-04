// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/navigation_console_logger.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/frame_type.h"
#include "content/public/browser/navigation_handle.h"

namespace subresource_filter {

// static
void NavigationConsoleLogger::LogMessageOnCommit(
    content::NavigationHandle* handle,
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message) {
  DCHECK(handle->IsInMainFrame());
  if (handle->GetNavigatingFrameType() ==
      content::FrameType::kFencedFrameRoot) {
    // TODO(crbug.com/1263541): Replace it with DCHECK once fenced frames use
    // the embedder's ContentSubresourceFilterThrottleManager.
    return;
  }
  if (handle->HasCommitted() && !handle->IsErrorPage()) {
    handle->GetRenderFrameHost()->AddMessageToConsole(level, message);
  } else {
    NavigationConsoleLogger::CreateIfNeededForNavigation(handle)
        ->commit_messages_.emplace_back(level, message);
  }
}

// static
NavigationConsoleLogger* NavigationConsoleLogger::CreateIfNeededForNavigation(
    content::NavigationHandle* handle) {
  DCHECK(handle->IsInMainFrame());
  DCHECK_NE(handle->GetNavigatingFrameType(),
            content::FrameType::kFencedFrameRoot);
  return GetOrCreateForNavigationHandle(*handle);
}

NavigationConsoleLogger::~NavigationConsoleLogger() = default;

NavigationConsoleLogger::NavigationConsoleLogger(
    content::NavigationHandle& handle)
    : content::WebContentsObserver(handle.GetWebContents()), handle_(&handle) {}

void NavigationConsoleLogger::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle != handle_)
    return;

  // The main frame navigation has finished.
  if (handle->HasCommitted() && !handle->IsErrorPage()) {
    for (const auto& message : commit_messages_) {
      handle->GetRenderFrameHost()->AddMessageToConsole(message.first,
                                                        message.second);
    }
  }
  // Deletes |this|.
  DeleteForNavigationHandle(*handle);
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationConsoleLogger);

}  // namespace subresource_filter
