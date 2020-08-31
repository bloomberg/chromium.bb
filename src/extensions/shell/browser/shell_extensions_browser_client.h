// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class PrefService;

namespace extensions {

class ExtensionsAPIClient;

// An ExtensionsBrowserClient that supports a single content::BrowserContext
// with no related incognito context.
// Must be initialized via InitWithBrowserContext() once the BrowserContext is
// created.
class ShellExtensionsBrowserClient : public ExtensionsBrowserClient {
 public:
  ShellExtensionsBrowserClient();
  ~ShellExtensionsBrowserClient() override;

  // ExtensionsBrowserClient overrides:
  bool IsShuttingDown() override;
  bool AreExtensionsDisabled(const base::CommandLine& command_line,
                             content::BrowserContext* context) override;
  bool IsValidContext(content::BrowserContext* context) override;
  bool IsSameContext(content::BrowserContext* first,
                     content::BrowserContext* second) override;
  bool HasOffTheRecordContext(content::BrowserContext* context) override;
  content::BrowserContext* GetOffTheRecordContext(
      content::BrowserContext* context) override;
  content::BrowserContext* GetOriginalContext(
      content::BrowserContext* context) override;
#if defined(OS_CHROMEOS)
  std::string GetUserIdHashFromContext(
      content::BrowserContext* context) override;
#endif
  bool IsGuestSession(content::BrowserContext* context) const override;
  bool IsExtensionIncognitoEnabled(
      const std::string& extension_id,
      content::BrowserContext* context) const override;
  bool CanExtensionCrossIncognito(
      const Extension* extension,
      content::BrowserContext* context) const override;
  base::FilePath GetBundleResourcePath(
      const network::ResourceRequest& request,
      const base::FilePath& extension_resources_path,
      int* resource_id) const override;
  void LoadResourceFromResourceBundle(
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      const base::FilePath& resource_relative_path,
      int resource_id,
      const std::string& content_security_policy,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      bool send_cors_header) override;
  bool AllowCrossRendererResourceLoad(const GURL& url,
                                      blink::mojom::ResourceType resource_type,
                                      ui::PageTransition page_transition,
                                      int child_id,
                                      bool is_incognito,
                                      const Extension* extension,
                                      const ExtensionSet& extensions,
                                      const ProcessMap& process_map) override;
  PrefService* GetPrefServiceForContext(
      content::BrowserContext* context) override;
  void GetEarlyExtensionPrefsObservers(
      content::BrowserContext* context,
      std::vector<EarlyExtensionPrefsObserver*>* observers) const override;
  ProcessManagerDelegate* GetProcessManagerDelegate() const override;
  std::unique_ptr<ExtensionHostDelegate> CreateExtensionHostDelegate() override;
  bool DidVersionUpdate(content::BrowserContext* context) override;
  void PermitExternalProtocolHandler() override;
  bool IsInDemoMode() override;
  bool IsScreensaverInDemoMode(const std::string& app_id) override;
  bool IsRunningInForcedAppMode() override;
  bool IsAppModeForcedForApp(const ExtensionId& extension_id) override;
  bool IsLoggedInAsPublicAccount() override;
  ExtensionSystemProvider* GetExtensionSystemFactory() override;
  void RegisterBrowserInterfaceBindersForFrame(
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) const override;
  std::unique_ptr<RuntimeAPIDelegate> CreateRuntimeAPIDelegate(
      content::BrowserContext* context) const override;
  const ComponentExtensionResourceManager*
  GetComponentExtensionResourceManager() override;
  void BroadcastEventToRenderers(
      events::HistogramValue histogram_value,
      const std::string& event_name,
      std::unique_ptr<base::ListValue> args,
      bool dispatch_to_off_the_record_profiles) override;
  ExtensionCache* GetExtensionCache() override;
  bool IsBackgroundUpdateAllowed() override;
  bool IsMinBrowserVersionSupported(const std::string& min_version) override;
  ExtensionWebContentsObserver* GetExtensionWebContentsObserver(
      content::WebContents* web_contents) override;
  KioskDelegate* GetKioskDelegate() override;
  bool IsLockScreenContext(content::BrowserContext* context) override;
  std::string GetApplicationLocale() override;
  std::string GetUserAgent() const override;

  // |context| is the single BrowserContext used for IsValidContext().
  // |pref_service| is used for GetPrefServiceForContext().
  void InitWithBrowserContext(content::BrowserContext* context,
                              PrefService* pref_service);

  // Sets the API client.
  void SetAPIClientForTest(ExtensionsAPIClient* api_client);

 private:
  // The single BrowserContext for app_shell. Not owned. Must be initialized
  // when ready by calling InitWithBrowserContext().
  content::BrowserContext* browser_context_ = nullptr;

  // The PrefService for |browser_context_|. Not owned. Must be initialized when
  // ready by calling InitWithBrowserContext().
  PrefService* pref_service_ = nullptr;

  // Support for extension APIs.
  std::unique_ptr<ExtensionsAPIClient> api_client_;

  // The extension cache used for download and installation.
  std::unique_ptr<ExtensionCache> extension_cache_;

  std::unique_ptr<KioskDelegate> kiosk_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionsBrowserClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSIONS_BROWSER_CLIENT_H_
