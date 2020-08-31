// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_CHROME_CONTENT_BROWSER_CLIENT_PLUGINS_PART_H_
#define CHROME_BROWSER_PLUGINS_CHROME_CONTENT_BROWSER_CLIENT_PLUGINS_PART_H_

#include "base/macros.h"
#include "chrome/browser/chrome_content_browser_client_parts.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/common/socket_permission_request.h"

namespace plugins {

// Implements the plugins portion of ChromeContentBrowserClient.
class ChromeContentBrowserClientPluginsPart
    : public ChromeContentBrowserClientParts {
 public:
  ChromeContentBrowserClientPluginsPart();
  ~ChromeContentBrowserClientPluginsPart() override;

  // Corresponding to the ChromeContentBrowserClient function of the same name.
  static bool IsPluginAllowedToUseDevChannelAPIs(
      content::BrowserContext* browser_context,
      const GURL& url);

  static bool AllowPepperSocketAPI(
      content::BrowserContext* browser_context,
      const GURL& url,
      bool private_api,
      const content::SocketPermissionRequest* params);

  static bool IsPepperVpnProviderAPIAllowed(
      content::BrowserContext* browser_context,
      const GURL& url);

  static bool IsPluginAllowedToCallRequestOSFileHandle(
      content::BrowserContext* browser_context,
      const GURL& url);

  static void DidCreatePpapiPlugin(content::BrowserPpapiHost* browser_host);

 private:
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentBrowserClientPluginsPart);
};

}  // namespace plugins

#endif  // CHROME_BROWSER_PLUGINS_CHROME_CONTENT_BROWSER_CLIENT_PLUGINS_PART_H_
