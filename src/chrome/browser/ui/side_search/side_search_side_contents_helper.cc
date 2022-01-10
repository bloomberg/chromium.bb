// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_side_contents_helper.h"

#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_metrics.h"
#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace {

constexpr char kSideSearchQueryParam[] = "sidesearch";
constexpr char kSideSearchVersion[] = "1";

#if !defined(OS_CHROMEOS)
constexpr char kChromeOSUserAgent[] =
    "Mozilla/5.0 (X11; CrOS x86_64 14233.0.0) AppleWebKit/537.36 (KHTML, like "
    "Gecko) Chrome/96.0.4650.0 Safari/537.36";
#endif  // !defined(OS_CHROMEOS)

class SideSearchContentsThrottle : public content::NavigationThrottle {
 public:
  explicit SideSearchContentsThrottle(
      content::NavigationHandle* navigation_handle)
      : NavigationThrottle(navigation_handle) {}

  // NavigationThrottle overrides.
  ThrottleCheckResult WillStartRequest() override {
    return HandleSidePanelRequest();
  }
  ThrottleCheckResult WillRedirectRequest() override {
    return HandleSidePanelRequest();
  }
  const char* GetNameForLogging() override {
    return "SideSearchContentsThrottle";
  }

 private:
  ThrottleCheckResult HandleSidePanelRequest() {
    const auto& url = navigation_handle()->GetURL();

    // Allow Google search navigations to proceed in the side panel.
    auto* config = SideSearchConfig::Get(
        navigation_handle()->GetWebContents()->GetBrowserContext());
    DCHECK(config);
    if (config->ShouldNavigateInSidePanel(url)) {
      return NavigationThrottle::PROCEED;
    }

    // Route all non-Google search URLs to the tab contents associated with this
    // side contents. This throttle will only be applied to WebContents objects
    // that are hosted in the side panel. Hence all intercepted navigations will
    // have a SideSearchContentsHelper.
    auto* side_contents_helper = SideSearchSideContentsHelper::FromWebContents(
        navigation_handle()->GetWebContents());
    DCHECK(side_contents_helper);
    side_contents_helper->NavigateInTabContents(
        content::OpenURLParams::FromNavigationHandle(navigation_handle()));
    return content::NavigationThrottle::CANCEL;
  }
};

}  // namespace

bool SideSearchSideContentsHelper::Delegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return false;
}

content::WebContents* SideSearchSideContentsHelper::Delegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return nullptr;
}

SideSearchSideContentsHelper::~SideSearchSideContentsHelper() {
  if (web_contents())
    web_contents()->SetDelegate(nullptr);
  MaybeRecordMetricsPerJourney();
}

std::unique_ptr<content::NavigationThrottle>
SideSearchSideContentsHelper::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  // Only add a SideSearchContentsThrottle for side contentses.
  auto* helper =
      SideSearchSideContentsHelper::FromWebContents(handle->GetWebContents());
  if (!helper)
    return nullptr;
  return std::make_unique<SideSearchContentsThrottle>(handle);
}

void SideSearchSideContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // DidFinishNavigation triggers even if the navigation has been cancelled by
  // the throttle. Early return if this is the case.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  const auto& url = navigation_handle->GetURL();
  DCHECK(GetConfig()->ShouldNavigateInSidePanel(url));
  DCHECK(delegate_);
  delegate_->LastSearchURLUpdated(url);

  RecordSideSearchNavigation(
      SideSearchNavigationType::kNavigationCommittedWithinSideSearch);
  ++navigation_within_side_search_count_;
}

void SideSearchSideContentsHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  DCHECK(delegate_);
  return delegate_->SidePanelProcessGone();
}

bool SideSearchSideContentsHelper::CanDragEnter(
    content::WebContents* source,
    const content::DropData& data,
    blink::DragOperationsMask operations_allowed) {
  return false;
}

bool SideSearchSideContentsHelper::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  DCHECK(delegate_);
  return delegate_->HandleKeyboardEvent(source, event);
}

content::WebContents* SideSearchSideContentsHelper::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  DCHECK(delegate_);
  return delegate_->OpenURLFromTab(source, params);
}

void SideSearchSideContentsHelper::NavigateInTabContents(
    const content::OpenURLParams& params) {
  DCHECK(delegate_);
  delegate_->NavigateInTabContents(params);
  RecordSideSearchNavigation(SideSearchNavigationType::kRedirectionToTab);
  ++redirection_to_tab_count_;
}

void SideSearchSideContentsHelper::LoadURL(const GURL& url) {
  DCHECK(GetConfig()->ShouldNavigateInSidePanel(url));

  // Do not reload the side contents if already navigated to `url`.
  if (web_contents()->GetLastCommittedURL() == url)
    return;

  GURL side_search_url =
      net::AppendQueryParameter(url, kSideSearchQueryParam, kSideSearchVersion);
  content::NavigationController::LoadURLParams load_url_params(side_search_url);

  // Fake the user agent on non ChromeOS systems to allow for development and
  // testing. This is needed as the side search SRP is only served to ChromeOS
  // user agents.
#if !defined(OS_CHROMEOS)
  load_url_params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  load_url_params.override_user_agent =
      content::NavigationController::UA_OVERRIDE_TRUE;
#endif  // defined(OS_CHROMEOS)

  web_contents()->GetController().LoadURLWithParams(load_url_params);

  MaybeRecordMetricsPerJourney();
  has_loaded_url_ = true;
}

void SideSearchSideContentsHelper::SetDelegate(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

SideSearchSideContentsHelper::SideSearchSideContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsUserData<SideSearchSideContentsHelper>(*web_contents),
      webui_load_timer_(web_contents,
                        "SideSearch.LoadDocumentTime",
                        "SideSearch.LoadCompletedTime") {
  Observe(web_contents);

#if !defined(OS_CHROMEOS)
  web_contents->SetUserAgentOverride(
      blink::UserAgentOverride::UserAgentOnly(kChromeOSUserAgent), true);
  web_contents->SetRendererInitiatedUserAgentOverrideOption(
      content::NavigationController::UA_OVERRIDE_TRUE);
#endif  // !defined(OS_CHROMEOS)

  web_contents->SetDelegate(this);
}

void SideSearchSideContentsHelper::MaybeRecordMetricsPerJourney() {
  // Do not record metrics if url has not loaded.
  if (!has_loaded_url_)
    return;

  RecordNavigationCommittedWithinSideSearchCountPerJourney(
      navigation_within_side_search_count_);
  RecordRedirectionToTabCountPerJourney(redirection_to_tab_count_);
  navigation_within_side_search_count_ = 0;
  redirection_to_tab_count_ = 0;
}

SideSearchConfig* SideSearchSideContentsHelper::GetConfig() {
  return SideSearchConfig::Get(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideSearchSideContentsHelper);
