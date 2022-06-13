// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/webui/web_ui_provider.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/common/channel_info.h"

namespace web_ui {

syncer::SyncService* GetSyncServiceForWebUI(web::WebUIIOS* web_ui) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  return SyncServiceFactory::GetForBrowserState(
      browser_state->GetOriginalChromeBrowserState());
}

std::string GetChannelString() {
  return ::GetChannelString();
}

version_info::Channel GetChannel() {
  return ::GetChannel();
}

}  // namespace web_ui
