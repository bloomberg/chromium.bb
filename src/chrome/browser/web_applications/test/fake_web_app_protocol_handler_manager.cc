// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_protocol_handler_manager.h"

namespace web_app {

FakeWebAppProtocolHandlerManager::FakeWebAppProtocolHandlerManager(
    Profile* profile)
    : WebAppProtocolHandlerManager(profile) {}

FakeWebAppProtocolHandlerManager::~FakeWebAppProtocolHandlerManager() = default;

void FakeWebAppProtocolHandlerManager::RegisterProtocolHandler(
    const AppId& app_id,
    const apps::ProtocolHandlerInfo& protocol_handler) {
  protocol_handlers_[app_id].push_back(protocol_handler);
}

std::vector<apps::ProtocolHandlerInfo>
FakeWebAppProtocolHandlerManager::GetAppProtocolHandlerInfos(
    const std::string& app_id) const {
  auto app_protocol_handlers = protocol_handlers_.find(app_id);
  return app_protocol_handlers != protocol_handlers_.end()
             ? app_protocol_handlers->second
             : std::vector<apps::ProtocolHandlerInfo>();
}

}  // namespace web_app
