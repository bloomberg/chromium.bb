// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_MANAGER_DELEGATE_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_MANAGER_DELEGATE_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class BrowserContext;
}

class WebEngineDevToolsManagerDelegate
    : public content::DevToolsManagerDelegate {
 public:
  explicit WebEngineDevToolsManagerDelegate(
      content::BrowserContext* browser_context);
  ~WebEngineDevToolsManagerDelegate() override;

  // DevToolsManagerDelegate implementation.
  content::BrowserContext* GetDefaultBrowserContext() override;
  void ClientAttached(content::DevToolsAgentHost* agent_host,
                      content::DevToolsAgentHostClient* client) override;
  void ClientDetached(content::DevToolsAgentHost* agent_host,
                      content::DevToolsAgentHostClient* client) override;

 private:
  content::BrowserContext* const browser_context_;
  base::flat_set<content::DevToolsAgentHostClient*> clients_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineDevToolsManagerDelegate);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_MANAGER_DELEGATE_H_