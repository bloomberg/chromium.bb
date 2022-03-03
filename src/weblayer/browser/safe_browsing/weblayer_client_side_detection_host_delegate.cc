// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/weblayer_client_side_detection_host_delegate.h"

#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/content/browser/client_side_detection_service.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/browser_process.h"
#include "weblayer/browser/safe_browsing/client_side_detection_service_factory.h"
#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

namespace weblayer {

WebLayerClientSideDetectionHostDelegate::
    WebLayerClientSideDetectionHostDelegate(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

WebLayerClientSideDetectionHostDelegate::
    ~WebLayerClientSideDetectionHostDelegate() = default;

bool WebLayerClientSideDetectionHostDelegate::
    HasSafeBrowsingUserInteractionObserver() {
  return false;
}

PrefService* WebLayerClientSideDetectionHostDelegate::GetPrefs() {
  BrowserContextImpl* browser_context_impl =
      static_cast<BrowserContextImpl*>(web_contents_->GetBrowserContext());
  DCHECK(browser_context_impl);
  return browser_context_impl->pref_service();
}

scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
WebLayerClientSideDetectionHostDelegate::GetSafeBrowsingDBManager() {
  SafeBrowsingService* sb_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return sb_service->GetSafeBrowsingDBManager();
}

scoped_refptr<safe_browsing::BaseUIManager>
WebLayerClientSideDetectionHostDelegate::GetSafeBrowsingUIManager() {
  SafeBrowsingService* sb_service =
      BrowserProcess::GetInstance()->GetSafeBrowsingService();
  return sb_service->GetSafeBrowsingUIManager();
}

safe_browsing::ClientSideDetectionService*
WebLayerClientSideDetectionHostDelegate::GetClientSideDetectionService() {
  return ClientSideDetectionServiceFactory::GetForBrowserContext(
      web_contents_->GetBrowserContext());
}

void WebLayerClientSideDetectionHostDelegate::AddReferrerChain(
    safe_browsing::ClientPhishingRequest* verdict,
    GURL current_url) {}

}  // namespace weblayer
