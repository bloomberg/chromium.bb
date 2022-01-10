// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_
#define CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_

#include "build/chromeos_buildflags.h"
#include "extensions/browser/api/extensions_api_client.h"

namespace extensions {

class ChromeAutomationInternalApiDelegate;
class ChromeMetricsPrivateDelegate;
class ClipboardExtensionHelper;

// Extra support for extensions APIs in Chrome.
class ChromeExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  ChromeExtensionsAPIClient();

  ChromeExtensionsAPIClient(const ChromeExtensionsAPIClient&) = delete;
  ChromeExtensionsAPIClient& operator=(const ChromeExtensionsAPIClient&) =
      delete;

  ~ChromeExtensionsAPIClient() override;

  // ExtensionsApiClient implementation.
  void AddAdditionalValueStoreCaches(
      content::BrowserContext* context,
      const scoped_refptr<value_store::ValueStoreFactory>& factory,
      const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
          observers,
      std::map<settings_namespace::Namespace, ValueStoreCache*>* caches)
      override;
  void AttachWebContentsHelpers(content::WebContents* web_contents) const
      override;
  bool ShouldHideResponseHeader(const GURL& url,
                                const std::string& header_name) const override;
  bool ShouldHideBrowserNetworkRequest(
      content::BrowserContext* context,
      const WebRequestInfo& request) const override;
  void NotifyWebRequestWithheld(int render_process_id,
                                int render_frame_id,
                                const ExtensionId& extension_id) override;
  void UpdateActionCount(content::BrowserContext* context,
                         const ExtensionId& extension_id,
                         int tab_id,
                         int action_count,
                         bool clear_badge_text) override;
  void ClearActionCount(content::BrowserContext* context,
                        const Extension& extension) override;
  AppViewGuestDelegate* CreateAppViewGuestDelegate() const override;
  ExtensionOptionsGuestDelegate* CreateExtensionOptionsGuestDelegate(
      ExtensionOptionsGuest* guest) const override;
  std::unique_ptr<guest_view::GuestViewManagerDelegate>
  CreateGuestViewManagerDelegate(
      content::BrowserContext* context) const override;
  std::unique_ptr<MimeHandlerViewGuestDelegate>
  CreateMimeHandlerViewGuestDelegate(
      MimeHandlerViewGuest* guest) const override;
  WebViewGuestDelegate* CreateWebViewGuestDelegate(
      WebViewGuest* web_view_guest) const override;
  WebViewPermissionHelperDelegate* CreateWebViewPermissionHelperDelegate(
      WebViewPermissionHelper* web_view_permission_helper) const override;
  scoped_refptr<ContentRulesRegistry> CreateContentRulesRegistry(
      content::BrowserContext* browser_context,
      RulesCacheDelegate* cache_delegate) const override;
  std::unique_ptr<DevicePermissionsPrompt> CreateDevicePermissionsPrompt(
      content::WebContents* web_contents) const override;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  bool ShouldAllowDetachingUsb(int vid, int pid) const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<VirtualKeyboardDelegate> CreateVirtualKeyboardDelegate(
      content::BrowserContext* browser_context) const override;
  ManagementAPIDelegate* CreateManagementAPIDelegate() const override;
  std::unique_ptr<SupervisedUserExtensionsDelegate>
  CreateSupervisedUserExtensionsDelegate() const override;

  std::unique_ptr<DisplayInfoProvider> CreateDisplayInfoProvider()
      const override;
  MetricsPrivateDelegate* GetMetricsPrivateDelegate() override;
  FileSystemDelegate* GetFileSystemDelegate() override;
  MessagingDelegate* GetMessagingDelegate() override;
  FeedbackPrivateDelegate* GetFeedbackPrivateDelegate() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MediaPerceptionAPIDelegate* GetMediaPerceptionAPIDelegate() override;
  NonNativeFileSystemDelegate* GetNonNativeFileSystemDelegate() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  void SaveImageDataToClipboard(
      std::vector<uint8_t> image_data,
      api::clipboard::ImageType type,
      AdditionalDataItemList additional_items,
      base::OnceClosure success_callback,
      base::OnceCallback<void(const std::string&)> error_callback) override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  AutomationInternalApiDelegate* GetAutomationInternalApiDelegate() override;
  std::vector<KeyedServiceBaseFactory*> GetFactoryDependencies() override;

 private:
  std::unique_ptr<ChromeMetricsPrivateDelegate> metrics_private_delegate_;
  std::unique_ptr<FileSystemDelegate> file_system_delegate_;
  std::unique_ptr<MessagingDelegate> messaging_delegate_;
  std::unique_ptr<FeedbackPrivateDelegate> feedback_private_delegate_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<MediaPerceptionAPIDelegate> media_perception_api_delegate_;
  std::unique_ptr<NonNativeFileSystemDelegate> non_native_file_system_delegate_;
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  std::unique_ptr<ClipboardExtensionHelper> clipboard_extension_helper_;
#endif
  std::unique_ptr<extensions::ChromeAutomationInternalApiDelegate>
      extensions_automation_api_delegate_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CHROME_EXTENSIONS_API_CLIENT_H_
