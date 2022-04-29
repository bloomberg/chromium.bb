// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_tab_contents_helper.h"

#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/side_search/side_search_config.h"
#include "chrome/browser/ui/side_search/side_search_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/tab_helper.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

SideSearchTabContentsHelper::~SideSearchTabContentsHelper() = default;

void SideSearchTabContentsHelper::NavigateInTabContents(
    const content::OpenURLParams& params) {
  side_panel_initiated_redirect_info_ = SidePanelRedirectInfo{
      params.url, ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                               params.transition)};

  web_contents()->GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kInteraction);
  web_contents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(params));
}

void SideSearchTabContentsHelper::LastSearchURLUpdated(const GURL& url) {
  DCHECK(GetConfig()->ShouldNavigateInSidePanel(url));
  last_search_url_ = url;
}

bool SideSearchTabContentsHelper::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return delegate_ ? delegate_->HandleKeyboardEvent(source, event) : false;
}

content::WebContents* SideSearchTabContentsHelper::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return delegate_ ? delegate_->OpenURLFromTab(source, params) : nullptr;
}

void SideSearchTabContentsHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // Reset the side panel redirect info if the current navigation does not
  // belong to the side panel initiated navigation shain.
  DCHECK(!navigation_handle->GetRedirectChain().empty());
  if (side_panel_initiated_redirect_info_ &&
      navigation_handle->GetRedirectChain()[0] !=
          side_panel_initiated_redirect_info_->initiated_redirect_url) {
    side_panel_initiated_redirect_info_.reset();
  }
}

void SideSearchTabContentsHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  const auto& url = navigation_handle->GetURL();
  auto* config = GetConfig();

  if (config->ShouldNavigateInSidePanel(url)) {
    returned_to_previous_srp_ = navigation_handle->GetPageTransition() &
                                ui::PAGE_TRANSITION_FORWARD_BACK;

    // Capture the URL here in case the side contents is closed before the
    // navigation completes.
    last_search_url_ = url;

    // If the navigation to a search results page succeeds we should update the
    // side panel availability bit accordingly.
    // TODO(tluk): If we continue to use a service check for side search SRP
    // availability independent of successfully committing to the search page
    // in the main tab it should be done during idle time to avoid regressing
    // page load metrics.
    config->set_is_side_panel_srp_available(true);

    if (side_panel_contents_)
      UpdateSideContentsNavigation();
  }
}

void SideSearchTabContentsHelper::OnSideSearchConfigChanged() {
  ClearHelperState();
}

void SideSearchTabContentsHelper::SidePanelProcessGone() {
  ClearSidePanelContents();
  // For state-per-tab we want to toggle the helper closed to ensure its toggled
  // state is updated correctly in the case the renderer crashes but it is not
  // currently being hosted in the side panel.
  toggled_open_ = false;
  if (delegate_)
    delegate_->SidePanelAvailabilityChanged(true);
}

content::WebContents* SideSearchTabContentsHelper::GetSidePanelContents() {
  if (!side_panel_contents_)
    CreateSidePanelContents();

  DCHECK(side_panel_contents_);
  UpdateSideContentsNavigation();
  return side_panel_contents_.get();
}

void SideSearchTabContentsHelper::ClearSidePanelContents() {
  // It is safe to reset this here as any views::WebViews hosting this
  // WebContents will clear their reference to this away during its destruction.
  side_panel_contents_.reset();
}

bool SideSearchTabContentsHelper::CanShowSidePanelForCommittedNavigation() {
  const GURL& url = web_contents()->GetLastCommittedURL();
  return last_search_url_ && GetConfig()->CanShowSidePanelForURL(url) &&
         GetConfig()->is_side_panel_srp_available();
}

void SideSearchTabContentsHelper::SetDelegate(
    base::WeakPtr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void SideSearchTabContentsHelper::SetSidePanelContentsForTesting(
    std::unique_ptr<content::WebContents> side_panel_contents) {
  side_panel_contents_ = std::move(side_panel_contents);
  SideSearchSideContentsHelper::CreateForWebContents(
      side_panel_contents_.get());
  GetSideContentsHelper()->SetDelegate(this);
}

SideSearchTabContentsHelper::SideSearchTabContentsHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SideSearchTabContentsHelper>(*web_contents) {
  config_observation_.Observe(GetConfig());
}

SideSearchSideContentsHelper*
SideSearchTabContentsHelper::GetSideContentsHelper() {
  DCHECK(side_panel_contents_);
  return SideSearchSideContentsHelper::FromWebContents(
      side_panel_contents_.get());
}

void SideSearchTabContentsHelper::CreateSidePanelContents() {
  DCHECK(!side_panel_contents_);
  side_panel_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(
          web_contents()->GetBrowserContext(), nullptr));

  // Apply a transparent background color so that we fallback to the hosting
  // side panel view's background color.
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      side_panel_contents_.get(), SK_ColorTRANSPARENT);

  task_manager::WebContentsTags::CreateForTabContents(
      side_panel_contents_.get());

  // Set helpers required for the side contents. We must add relevant tab
  // helpers here explicitly as TabHelpers::AttachTabHelpers() is only called
  // for tab WebContents. If called here it would add helpers that do not make
  // sense / are not relevant for non-tab WebContents.
  PrefsTabHelper::CreateForWebContents(side_panel_contents_.get());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::TabHelper::CreateForWebContents(side_panel_contents_.get());
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  chrome::InitializePageLoadMetricsForWebContents(side_panel_contents_.get());

  SideSearchSideContentsHelper::CreateForWebContents(
      side_panel_contents_.get());
  GetSideContentsHelper()->SetDelegate(this);
}

void SideSearchTabContentsHelper::UpdateSideContentsNavigation() {
  DCHECK(side_panel_contents_);
  // Only update the side panel contents with the latest `last_search_url_` if
  // present.
  if (last_search_url_ && GetConfig()->is_side_panel_srp_available()) {
    GetSideContentsHelper()->LoadURL(last_search_url_.value());
    side_search::MaybeSaveSideSearchTabSessionData(web_contents());
  }
}

void SideSearchTabContentsHelper::ClearHelperState() {
  toggled_open_ = false;
  simple_loader_.reset();
  last_search_url_.reset();
  returned_to_previous_srp_ = false;
  toggled_open_ = false;

  // Notify the side panel after resetting the above state but before clearing
  // away the side panel WebContents. This will close the side panel if it's
  // currently open.
  if (delegate_)
    delegate_->SidePanelAvailabilityChanged(true);

  ClearSidePanelContents();
}

void SideSearchTabContentsHelper::TestSRPAvailability() {
  if (GetConfig()->is_side_panel_srp_available())
    return;
  // TODO(tluk): Add rate limiting to the SRP test to permanently disable the
  // feature for a given session if the availability check fails enough times.
  DCHECK(last_search_url_.has_value());
  DCHECK(GetConfig()->ShouldNavigateInSidePanel(last_search_url_.value()));
  auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("side_search_availability_test", R"(
        semantics {
          sender: "Side Search Tab Helper"
          description:
            "After the user has successfully navigated to a search results "
            "page (SRP) belonging to their set default search provider, a HEAD "
            "request is made to the side search SRP URL.\n"
            "The side search SRP URL is generated by taking the original SRP "
            "URL and appending the side search param specified in the search "
            "engine's prepopulated_engines.json entry.\n"
            "This is only done once per session for the currently set default "
            "search engine to check the availability of the side search SRP "
            "before enabling the feature. This is also gated on the current "
            "default search engine signalling participation in the feature "
            "with appropriate updates to its prepopulated_engines.json entry."
          trigger:
            "After the user has successfully committed a navigation to a "
            "default search engine SRP in a tab contents and the availability "
            "bit for the default search engine has not already been set for "
            "this session."
          data:
            "The HEAD request includes the original search URL with the "
            "addition of the side search header but no PII data / cookies."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "None"
          chrome_policy {
            SideSearchEnabled {
              SideSearchEnabled: false
            }
          }
        })");
  auto url_loader_factory = web_contents()
                                ->GetBrowserContext()
                                ->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
  auto request = std::make_unique<network::ResourceRequest>();
  // Ensure cookies are not propagated with the request.
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->url = last_search_url_.value();
  // Make a HEAD request to avoid generating an actual SRP page when checking
  // for availability of the side panel SRP.
  request->method = net::HttpRequestHeaders::kHeadMethod;
  simple_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  simple_loader_->DownloadHeadersOnly(
      url_loader_factory.get(),
      base::BindOnce(&SideSearchTabContentsHelper::OnResponseLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SideSearchTabContentsHelper::OnResponseLoaded(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  GetConfig()->set_is_side_panel_srp_available(simple_loader_->NetError() ==
                                               net::OK);

  // The test for availability is performed async so alert `delegate_` that the
  // side panel SRP is available to give it the opportunity to update
  // appropriately.
  if (delegate_)
    delegate_->SidePanelAvailabilityChanged(false);
}

SideSearchConfig* SideSearchTabContentsHelper::GetConfig() {
  return SideSearchConfig::Get(web_contents()->GetBrowserContext());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SideSearchTabContentsHelper);
