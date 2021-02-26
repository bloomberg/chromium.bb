// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/well_known_change_password_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "components/ukm/ios/ukm_url_recorder.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_change_password_url_service_factory.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "net/base/mac/url_conversions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/page_transition_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::WellKnownChangePasswordTabHelper;

WellKnownChangePasswordTabHelper::WellKnownChangePasswordTabHelper(
    web::WebState* web_state)
    : web::WebStatePolicyDecider(web_state),
      web_state_(web_state),
      change_password_url_service_(
          IOSChromeChangePasswordUrlServiceFactory::GetForBrowserState(
              web_state->GetBrowserState())) {
  web_state->AddObserver(this);
}

WellKnownChangePasswordTabHelper::~WellKnownChangePasswordTabHelper() = default;

void WellKnownChangePasswordTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // |request_url_| is set when the first request goes to
  // .well-known-change-password. If the navigation url and |request_url_| are
  // equal, DidStartNavigation() is called for the .well-known/change-password
  // navigation. Otherwise a different navigation was started.
  if (!(request_url_.is_valid() &&
        request_url_ == navigation_context->GetUrl())) {
    processing_state_ = kInactive;
  }
}

web::WebStatePolicyDecider::PolicyDecision
WellKnownChangePasswordTabHelper::ShouldAllowRequest(
    NSURLRequest* request,
    const RequestInfo& request_info) {
  GURL request_url = net::GURLWithNSURL(request.URL);
  // The custom behaviour is only used if the .well-known/change-password
  // request if the request is the main frame opened in a new tab.
  if (processing_state_ == kWaitingForFirstRequest) {
    // Boolean order important, feature flag last. Otherwise it messes
    // with usage statistics of control and experimental group (UMA).
    if (request_info.target_frame_is_main &&
        ui::PageTransitionCoreTypeIs(request_info.transition_type,
                                     ui::PAGE_TRANSITION_TYPED) &&
        web_state_->GetLastCommittedURL().is_empty() &&  // empty tab history
        IsWellKnownChangePasswordUrl(request_url) &&
        base::FeatureList::IsEnabled(
            password_manager::features::kWellKnownChangePassword)) {
      request_url_ = request_url;
      change_password_url_service_->PrefetchURLs();
      auto url_loader_factory =
          web_state_->GetBrowserState()->GetSharedURLLoaderFactory();
      well_known_change_password_state_.FetchNonExistingResource(
          url_loader_factory.get(), request_url);
      processing_state_ = kWaitingForResponse;
    } else {
      processing_state_ = kInactive;
    }
  }

  return web::WebStatePolicyDecider::PolicyDecision::Allow();
}

void WellKnownChangePasswordTabHelper::ShouldAllowResponse(
    NSURLResponse* response,
    bool for_main_frame,
    web::WebStatePolicyDecider::PolicyDecisionCallback callback) {
  GURL url = net::GURLWithNSURL(response.URL);
  // True if the TabHelper expects the response from .well-known/change-password
  // and only that navigation was started.
  if (processing_state_ == kWaitingForResponse &&
      [response isKindOfClass:[NSHTTPURLResponse class]] &&
      base::FeatureList::IsEnabled(
          password_manager::features::kWellKnownChangePassword)) {
    processing_state_ = kResponesReceived;
    DCHECK(url.SchemeIsHTTPOrHTTPS());
    response_policy_callback_ = std::move(callback);

    well_known_change_password_state_.SetChangePasswordResponseCode(
        static_cast<NSHTTPURLResponse*>(response).statusCode);
  } else {
    std::move(callback).Run(
        web::WebStatePolicyDecider::PolicyDecision::Allow());
  }
}

void WellKnownChangePasswordTabHelper::RenderProcessGone(
    web::WebState* web_state) {
  if (!response_policy_callback_)
    return;
  std::move(response_policy_callback_)
      .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
}

void WellKnownChangePasswordTabHelper::WebStateDestroyed() {}

void WellKnownChangePasswordTabHelper::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
  if (!response_policy_callback_)
    return;
  std::move(response_policy_callback_)
      .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
}

void WellKnownChangePasswordTabHelper::OnProcessingFinished(bool is_supported) {
  if (!response_policy_callback_)
    return;
  if (is_supported) {
    std::move(response_policy_callback_)
        .Run(web::WebStatePolicyDecider::PolicyDecision::Allow());
    RecordMetric(WellKnownChangePasswordResult::kUsedWellKnownChangePassword);
  } else {
    std::move(response_policy_callback_)
        .Run(web::WebStatePolicyDecider::PolicyDecision::Cancel());
    GURL redirect_url =
        change_password_url_service_->GetChangePasswordUrl(request_url_);
    if (redirect_url.is_valid()) {
      RecordMetric(WellKnownChangePasswordResult::kFallbackToOverrideUrl);
      Redirect(redirect_url);
    } else {
      RecordMetric(WellKnownChangePasswordResult::kFallbackToOriginUrl);
      Redirect(request_url_.GetOrigin());
    }
  }
}

void WellKnownChangePasswordTabHelper::Redirect(const GURL& url) {
  // Making sure there was no other navigation started we could override.
  if (processing_state_ == kResponesReceived) {
    web::WebState::OpenURLParams params(url, web::Referrer(),
                                        WindowOpenDisposition::CURRENT_TAB,
                                        ui::PAGE_TRANSITION_LINK, false);
    web_state_->OpenURL(params);
  }
}

void WellKnownChangePasswordTabHelper::RecordMetric(
    WellKnownChangePasswordResult result) {
  ukm::SourceId source_id = ukm::GetSourceIdForWebStateDocument(web_state_);
  ukm::builders::PasswordManager_WellKnownChangePasswordResult(source_id)
      .SetWellKnownChangePasswordResult(static_cast<int64_t>(result))
      .Record(ukm::UkmRecorder::Get());
}

WEB_STATE_USER_DATA_KEY_IMPL(WellKnownChangePasswordTabHelper)
