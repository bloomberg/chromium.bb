// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/devtools_manager_delegate.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"

namespace content {

void DevToolsManagerDelegate::Inspect(DevToolsAgentHost* agent_host) {
}

std::string DevToolsManagerDelegate::GetTargetType(WebContents* wc) {
  return std::string();
}

std::string DevToolsManagerDelegate::GetTargetTitle(WebContents* wc) {
  return std::string();
}

std::string DevToolsManagerDelegate::GetTargetDescription(WebContents* wc) {
  return std::string();
}

bool DevToolsManagerDelegate::AllowInspectingRenderFrameHost(
    RenderFrameHost* rfh) {
  return true;
}

DevToolsAgentHost::List DevToolsManagerDelegate::RemoteDebuggingTargets() {
  return DevToolsAgentHost::GetOrCreateAll();
}

scoped_refptr<DevToolsAgentHost> DevToolsManagerDelegate::CreateNewTarget(
    const GURL& url) {
  return nullptr;
}

std::vector<BrowserContext*> DevToolsManagerDelegate::GetBrowserContexts() {
  return std::vector<BrowserContext*>();
}

BrowserContext* DevToolsManagerDelegate::GetDefaultBrowserContext() {
  return nullptr;
}

BrowserContext* DevToolsManagerDelegate::CreateBrowserContext() {
  return nullptr;
}

void DevToolsManagerDelegate::DisposeBrowserContext(BrowserContext*,
                                                    DisposeCallback callback) {
  std::move(callback).Run(false, "Browser Context disposal is not supported");
}

void DevToolsManagerDelegate::ClientAttached(DevToolsAgentHost* agent_host,
                                             DevToolsAgentHostClient* client) {}
void DevToolsManagerDelegate::ClientDetached(DevToolsAgentHost* agent_host,
                                             DevToolsAgentHostClient* client) {}

void DevToolsManagerDelegate::HandleCommand(
    DevToolsAgentHost* agent_host,
    DevToolsAgentHostClient* client,
    std::unique_ptr<base::DictionaryValue> command,
    const std::string& message,
    NotHandledCallback callback) {
  std::move(callback).Run(std::move(command), message);
}

std::string DevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return std::string();
}

bool DevToolsManagerDelegate::HasBundledFrontendResources() {
  return false;
}

bool DevToolsManagerDelegate::IsBrowserTargetDiscoverable() {
  return false;
}

DevToolsManagerDelegate::~DevToolsManagerDelegate() {
}

}  // namespace content
