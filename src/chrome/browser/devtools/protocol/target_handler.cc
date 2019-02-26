// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler.h"

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"
#include "chrome/browser/devtools/devtools_browser_context_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "content/public/browser/devtools_agent_host.h"

TargetHandler::TargetHandler(protocol::UberDispatcher* dispatcher) {
  protocol::Target::Dispatcher::wire(dispatcher, this);
}

TargetHandler::~TargetHandler() {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
}

protocol::Response TargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
        locations) {
  remote_locations_.clear();
  if (!locations)
    return protocol::Response::OK();

  for (size_t i = 0; i < locations->length(); ++i) {
    auto* item = locations->get(i);
    remote_locations_.insert(
        net::HostPortPair(item->GetHost(), item->GetPort()));
  }

  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
  return protocol::Response::OK();
}

protocol::Response TargetHandler::CreateTarget(
    const std::string& url,
    protocol::Maybe<int> width,
    protocol::Maybe<int> height,
    protocol::Maybe<std::string> browser_context_id,
    protocol::Maybe<bool> enable_begin_frame_control,
    std::string* out_target_id) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (browser_context_id.isJust()) {
    std::string profile_id = browser_context_id.fromJust();
    profile =
        DevToolsBrowserContextManager::GetInstance().GetProfileById(profile_id);
    if (!profile) {
      return protocol::Response::Error(
          "Failed to find browser context with id " + profile_id);
    }
  }

  NavigateParams params(profile, GURL(url), ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  Browser* target_browser = nullptr;
  // Find a browser to open a new tab.
  // We shouldn't use browser that is scheduled to close.
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() == profile &&
        !browser->IsAttemptingToCloseBrowser()) {
      target_browser = browser;
      break;
    }
  }
  if (target_browser) {
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    params.browser = target_browser;
  } else {
    params.disposition = WindowOpenDisposition::NEW_WINDOW;
  }

  Navigate(&params);
  if (!params.navigated_or_inserted_contents)
    return protocol::Response::Error("Failed to open a new tab");

  *out_target_id = content::DevToolsAgentHost::GetOrCreateFor(
                       params.navigated_or_inserted_contents)
                       ->GetId();
  return protocol::Response::OK();
}


