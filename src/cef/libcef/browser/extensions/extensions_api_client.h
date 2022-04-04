// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_API_CLIENT_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_API_CLIENT_H_

#include "components/value_store/value_store_factory.h"
#include "extensions/browser/api/extensions_api_client.h"

namespace extensions {

class CefExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  CefExtensionsAPIClient();

  // ExtensionsAPIClient implementation.
  AppViewGuestDelegate* CreateAppViewGuestDelegate() const override;
  std::unique_ptr<guest_view::GuestViewManagerDelegate>
  CreateGuestViewManagerDelegate(
      content::BrowserContext* context) const override;
  std::unique_ptr<MimeHandlerViewGuestDelegate>
  CreateMimeHandlerViewGuestDelegate(
      MimeHandlerViewGuest* guest) const override;
  void AttachWebContentsHelpers(
      content::WebContents* web_contents) const override;
  FileSystemDelegate* GetFileSystemDelegate() override;

  // Storage API support.

  // Add any additional value store caches (e.g. for chrome.storage.managed)
  // to |caches|. By default adds nothing.
  void AddAdditionalValueStoreCaches(
      content::BrowserContext* context,
      const scoped_refptr<value_store::ValueStoreFactory>& factory,
      SettingsChangedCallback observer,
      std::map<settings_namespace::Namespace, ValueStoreCache*>* caches)
      override;

 private:
  std::unique_ptr<FileSystemDelegate> file_system_delegate_;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSIONS_API_CLIENT_H_
