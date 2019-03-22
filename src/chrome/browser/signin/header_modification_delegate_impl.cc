// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/header_modification_delegate_impl.h"

#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_navigation_ui_data.h"

namespace signin {

HeaderModificationDelegateImpl::HeaderModificationDelegateImpl(
    content::ResourceContext* resource_context)
    : io_data_(ProfileIOData::FromResourceContext(resource_context)) {}

HeaderModificationDelegateImpl::~HeaderModificationDelegateImpl() = default;

bool HeaderModificationDelegateImpl::ShouldInterceptNavigation(
    content::NavigationUIData* navigation_ui_data) {
  if (io_data_->IsOffTheRecord())
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Note: InlineLoginUI uses an isolated request context and thus should
  // bypass the account consistency flow. See http://crbug.com/428396
  ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data);
  if (chrome_navigation_ui_data) {
    extensions::ExtensionNavigationUIData* extension_navigation_ui_data =
        chrome_navigation_ui_data->GetExtensionNavigationUIData();
    if (extension_navigation_ui_data &&
        extension_navigation_ui_data->is_web_view()) {
      return false;
    }
  }
#endif

  return true;
}

void HeaderModificationDelegateImpl::ProcessRequest(
    ChromeRequestAdapter* request_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  FixAccountConsistencyRequestHeader(request_adapter, redirect_url, io_data_);
}

void HeaderModificationDelegateImpl::ProcessResponse(
    ResponseAdapter* response_adapter,
    const GURL& redirect_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ProcessAccountConsistencyResponseHeaders(response_adapter, redirect_url,
                                           io_data_->IsOffTheRecord());
}

}  // namespace signin
