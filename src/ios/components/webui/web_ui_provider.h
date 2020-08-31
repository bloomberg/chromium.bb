// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_WEBUI_WEB_UI_PROVIDER_H_
#define IOS_COMPONENTS_WEBUI_WEB_UI_PROVIDER_H_

namespace syncer {
class SyncService;
}  // namespace syncer

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace web {
class WebUIIOS;
}  // namespace web

// Declares functions that must be implemented by the embedder, such as
// ios/chrome and ios/web_view.
namespace web_ui {

// Gets the SyncService of the underlying original profile. May return null.
syncer::SyncService* GetSyncServiceForWebUI(web::WebUIIOS* web_ui);

// Returns the app channel .
version_info::Channel GetChannel();

}  // namespace web_ui

#endif  // IOS_COMPONENTS_WEBUI_WEB_UI_PROVIDER_H_
