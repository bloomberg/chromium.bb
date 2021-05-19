// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_api_client.h"

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/extensions/api/storage/sync_value_store_cache.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"
#include "libcef/browser/extensions/pdf_web_contents_helper_client.h"
#include "libcef/browser/printing/print_view_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/pdf/browser/pdf_web_contents_helper.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"

namespace extensions {

CefExtensionsAPIClient::CefExtensionsAPIClient() {}

AppViewGuestDelegate* CefExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  // TODO(extensions): Implement to support Apps.
  NOTREACHED();
  return nullptr;
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
CefExtensionsAPIClient::CreateGuestViewManagerDelegate(
    content::BrowserContext* context) const {
  // The GuestViewManager instance associated with the returned Delegate, which
  // will be retrieved in the future via GuestViewManager::FromBrowserContext,
  // will be associated with the CefBrowserContext.
  return base::WrapUnique(
      new extensions::ExtensionsGuestViewManagerDelegate(context));
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
CefExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return base::WrapUnique(new CefMimeHandlerViewGuestDelegate(guest));
}

void CefExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  PrefsTabHelper::CreateForWebContents(web_contents);
  printing::CefPrintViewManager::CreateForWebContents(web_contents);

  CefExtensionWebContentsObserver::CreateForWebContents(web_contents);

  // Used by the PDF extension.
  pdf::PDFWebContentsHelper::CreateForWebContentsWithClient(
      web_contents, std::unique_ptr<pdf::PDFWebContentsHelperClient>(
                        new CefPDFWebContentsHelperClient()));

  // Used by the tabs extension API.
  zoom::ZoomController::CreateForWebContents(web_contents);
}

void CefExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<ValueStoreFactory>& factory,
    const scoped_refptr<base::ObserverListThreadSafe<SettingsObserver>>&
        observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {
  // Add support for chrome.storage.sync.
  // Because we don't support syncing with Google, we follow the behavior of
  // chrome.storage.sync as if Chrome were permanently offline, by using a local
  // store see: https://developer.chrome.com/apps/storage for more information
  (*caches)[settings_namespace::SYNC] = new cef::SyncValueStoreCache(factory);
}

}  // namespace extensions
