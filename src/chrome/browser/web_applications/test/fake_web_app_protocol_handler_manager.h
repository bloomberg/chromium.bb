// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "chrome/browser/web_applications/web_app_protocol_handler_manager.h"

namespace web_app {

// Fake implementation of WebAppProtocolHandlerManager
class FakeWebAppProtocolHandlerManager : public WebAppProtocolHandlerManager {
 public:
  explicit FakeWebAppProtocolHandlerManager(Profile* profile);
  ~FakeWebAppProtocolHandlerManager() override;

  void RegisterProtocolHandler(
      const AppId& app_id,
      const apps::ProtocolHandlerInfo& protocol_handler);

  std::vector<apps::ProtocolHandlerInfo> GetAppProtocolHandlerInfos(
      const std::string& app_id) const override;

 private:
  base::flat_map<AppId, std::vector<apps::ProtocolHandlerInfo>>
      protocol_handlers_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROTOCOL_HANDLER_MANAGER_H_
