// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_devtools_manager_delegate.h"

#include <stdint.h>

#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"

WebEngineDevToolsManagerDelegate::WebEngineDevToolsManagerDelegate(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  DCHECK(browser_context_);
}

WebEngineDevToolsManagerDelegate::~WebEngineDevToolsManagerDelegate() = default;

content::BrowserContext*
WebEngineDevToolsManagerDelegate::GetDefaultBrowserContext() {
  return browser_context_;
}

void WebEngineDevToolsManagerDelegate::ClientAttached(
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client) {
  bool did_insert = clients_.insert(client).second;

  // Make sure we don't receive notifications twice for the same client.
  DCHECK(did_insert);
}

void WebEngineDevToolsManagerDelegate::ClientDetached(
    content::DevToolsAgentHost* agent_host,
    content::DevToolsAgentHostClient* client) {
  size_t removed_elements = clients_.erase(client);
  DCHECK_EQ(removed_elements, 1u);
}
