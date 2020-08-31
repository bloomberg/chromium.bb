// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/components/security_interstitials/ios_blocking_page_controller_client.h"

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "components/security_interstitials/core/metrics_helper.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ios/web/public/security/web_interstitial.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace security_interstitials {

IOSBlockingPageControllerClient::IOSBlockingPageControllerClient(
    web::WebState* web_state,
    std::unique_ptr<security_interstitials::MetricsHelper> metrics_helper,
    const std::string& app_locale)
    : security_interstitials::ControllerClient(std::move(metrics_helper)),
      web_state_(web_state),
      web_interstitial_(nullptr),
      app_locale_(app_locale),
      weak_factory_(this) {
  web_state_->AddObserver(this);
}

IOSBlockingPageControllerClient::~IOSBlockingPageControllerClient() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
  }
}

void IOSBlockingPageControllerClient::SetWebInterstitial(
    web::WebInterstitial* web_interstitial) {
  web_interstitial_ = web_interstitial;
}

void IOSBlockingPageControllerClient::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

bool IOSBlockingPageControllerClient::CanLaunchDateAndTimeSettings() {
  return false;
}

void IOSBlockingPageControllerClient::LaunchDateAndTimeSettings() {
  NOTREACHED();
}

void IOSBlockingPageControllerClient::GoBack() {
  if (CanGoBack()) {
    web_state_->GetNavigationManager()->GoBack();
  } else {
    // Closing the tab synchronously is problematic since web state is heavily
    // involved in the operation and CloseWebState interrupts it, so call
    // CloseWebState asynchronously.
    base::PostTask(FROM_HERE, {web::WebThread::UI},
                   base::BindOnce(&IOSBlockingPageControllerClient::Close,
                                  weak_factory_.GetWeakPtr()));
  }
}

bool IOSBlockingPageControllerClient::CanGoBack() {
  return web_state_->GetNavigationManager()->CanGoBack();
}

bool IOSBlockingPageControllerClient::CanGoBackBeforeNavigation() {
  NOTREACHED();
  return false;
}

void IOSBlockingPageControllerClient::GoBackAfterNavigationCommitted() {
  NOTREACHED();
}

void IOSBlockingPageControllerClient::Proceed() {
  DCHECK(web_interstitial_);
  web_interstitial_->Proceed();
}

void IOSBlockingPageControllerClient::Reload() {
  web_state_->GetNavigationManager()->Reload(web::ReloadType::NORMAL,
                                             true /*check_for_repost*/);
}

void IOSBlockingPageControllerClient::OpenUrlInCurrentTab(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

void IOSBlockingPageControllerClient::OpenUrlInNewForegroundTab(
    const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, false));
}

const std::string& IOSBlockingPageControllerClient::GetApplicationLocale()
    const {
  return app_locale_;
}

PrefService* IOSBlockingPageControllerClient::GetPrefService() {
  return nullptr;
}

const std::string
IOSBlockingPageControllerClient::GetExtendedReportingPrefName() const {
  return std::string();
}

void IOSBlockingPageControllerClient::Close() {
  if (web_state_) {
    web_state_->CloseWebState();
  }
}

}  // namespace security_interstitials
