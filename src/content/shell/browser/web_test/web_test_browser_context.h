// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BROWSER_CONTEXT_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BROWSER_CONTEXT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/shell/browser/shell_browser_context.h"

namespace device {
class ScopedGeolocationOverrider;
}

namespace net {
class NetLog;
}

namespace content {

class BackgroundSyncController;
class DownloadManagerDelegate;
class WebTestBackgroundFetchDelegate;
class WebTestPermissionManager;
class WebTestPushMessagingService;
class PermissionControllerDelegate;
class PushMessagingService;

class WebTestBrowserContext final : public ShellBrowserContext {
 public:
  WebTestBrowserContext(bool off_the_record, net::NetLog* net_log);
  ~WebTestBrowserContext() override;

  // BrowserContext implementation.
  DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  PushMessagingService* GetPushMessagingService() override;
  PermissionControllerDelegate* GetPermissionControllerDelegate() override;
  BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  BackgroundSyncController* GetBackgroundSyncController() override;

  WebTestPermissionManager* GetWebTestPermissionManager();

 protected:
  ShellURLRequestContextGetter* CreateURLRequestContextGetter(
      ProtocolHandlerMap* protocol_handlers,
      URLRequestInterceptorScopedVector request_interceptors) override;

 private:
  std::unique_ptr<WebTestPushMessagingService> push_messaging_service_;
  std::unique_ptr<PermissionControllerDelegate> permission_manager_;
  std::unique_ptr<WebTestBackgroundFetchDelegate> background_fetch_delegate_;
  std::unique_ptr<BackgroundSyncController> background_sync_controller_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  DISALLOW_COPY_AND_ASSIGN(WebTestBrowserContext);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BROWSER_CONTEXT_H_
