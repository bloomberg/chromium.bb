// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/webui/web_ui_provider.h"

#include "components/version_info/channel.h"
#include "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "ios/components/webui/web_ui_provider.h"

namespace web_ui {

syncer::SyncService* GetSyncServiceForWebUI(web::WebUIIOS* web_ui) {
  ios_web_view::WebViewBrowserState* browser_state =
      ios_web_view::WebViewBrowserState::FromWebUIIOS(web_ui);
  return ios_web_view::WebViewProfileSyncServiceFactory::GetForBrowserState(
      browser_state->GetRecordingBrowserState());
}

version_info::Channel GetChannel() {
  return version_info::Channel::STABLE;
}

}  // namespace web_ui
