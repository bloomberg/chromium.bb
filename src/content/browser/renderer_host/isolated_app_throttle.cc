// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/isolated_app_throttle.h"

#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_exposed_isolation_info.h"
#include "content/common/navigation_params_utils.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_type.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"
#include "url/scheme_host_port.h"

namespace content {

namespace {

// Stores the origin of the isolated application that a WebContents is bound to.
// Every WebContents will have an instance of this class assigned to it during
// its first navigation, which will determine whether the WebContents hosts an
// isolated app for its lifetime. Note that activating an alternative frame tree
// (e.g. preloading or portals) will NOT override this state.
class WebContentsIsolationInfo
    : public WebContentsUserData<WebContentsIsolationInfo> {
 public:
  ~WebContentsIsolationInfo() override = default;

  bool is_isolated_application() { return isolated_origin_.has_value(); }

  const url::Origin& origin() {
    DCHECK(is_isolated_application());
    return isolated_origin_.value();
  }

 private:
  friend class WebContentsUserData<WebContentsIsolationInfo>;
  explicit WebContentsIsolationInfo(WebContents* web_contents,
                                    absl::optional<url::Origin> isolated_origin)
      : WebContentsUserData<WebContentsIsolationInfo>(*web_contents),
        isolated_origin_(isolated_origin) {}

  absl::optional<url::Origin> isolated_origin_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsIsolationInfo);

absl::optional<url::SchemeHostPort> GetTupleFromOptionalOrigin(
    const absl::optional<url::Origin>& origin) {
  if (origin.has_value()) {
    return origin->GetTupleOrPrecursorTupleIfOpaque();
  }
  return absl::nullopt;
}

}  // namespace

// static
std::unique_ptr<IsolatedAppThrottle>
IsolatedAppThrottle::MaybeCreateThrottleFor(NavigationHandle* handle) {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableIsolatedStorage)) {
    return std::make_unique<IsolatedAppThrottle>(handle);
  }
  return nullptr;
}

IsolatedAppThrottle::IsolatedAppThrottle(NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

IsolatedAppThrottle::~IsolatedAppThrottle() = default;

NavigationThrottle::ThrottleCheckResult
IsolatedAppThrottle::WillStartRequest() {
  bool requests_app_isolation = embedder_requests_app_isolation();
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  dest_origin_ = navigation_request->GetTentativeOriginAtRequestTime();

  // If this is the first navigation in this WebContents, save the isolation
  // state to validate future navigations.
  auto* web_contents_isolation_info = WebContentsIsolationInfo::FromWebContents(
      navigation_handle()->GetWebContents());
  if (!web_contents_isolation_info) {
    WebContentsIsolationInfo::CreateForWebContents(
        navigation_handle()->GetWebContents(),
        requests_app_isolation ? absl::make_optional(dest_origin_)
                               : absl::nullopt);
  }

  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (!frame_tree_node->is_on_initial_empty_document()) {
    prev_origin_ = frame_tree_node->current_origin();
  }

  return DoThrottle(requests_app_isolation, NavigationThrottle::BLOCK_REQUEST);
}

NavigationThrottle::ThrottleCheckResult
IsolatedAppThrottle::WillRedirectRequest() {
  // On redirects, the old destination origin becomes the new previous origin.
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  prev_origin_ = dest_origin_;
  dest_origin_ = navigation_request->GetTentativeOriginAtRequestTime();

  return DoThrottle(embedder_requests_app_isolation(),
                    NavigationThrottle::BLOCK_REQUEST);
}

NavigationThrottle::ThrottleCheckResult
IsolatedAppThrottle::WillProcessResponse() {
  // Update |dest_origin_| to point to the final origin, which may have changed
  // since the last WillStartRequest/WillRedirectRequest call.
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  dest_origin_ = navigation_request->GetOriginToCommit();
  auto* assigned_rfh = static_cast<RenderFrameHostImpl*>(
      navigation_request->GetRenderFrameHost());
  const WebExposedIsolationInfo& assigned_isolation_info =
      assigned_rfh->GetSiteInstance()->GetWebExposedIsolationInfo();

  return DoThrottle(assigned_isolation_info.is_isolated_application(),
                    NavigationThrottle::BLOCK_RESPONSE);
}

NavigationThrottle::ThrottleCheckResult IsolatedAppThrottle::DoThrottle(
    bool needs_app_isolation,
    NavigationThrottle::ThrottleAction block_action) {
  auto* web_contents_isolation_info = WebContentsIsolationInfo::FromWebContents(
      navigation_handle()->GetWebContents());
  DCHECK(web_contents_isolation_info);

  // Block navigations into isolated apps from non-isolated app contexts.
  if (!web_contents_isolation_info->is_isolated_application()) {
    return needs_app_isolation ? block_action : NavigationThrottle::PROCEED;
  }

  // We want the following origin checks to be a bit more permissive than
  // usual. In particular, if the isolation, previous, or destination origins
  // are opaque, we want to use their precursor tuple for "origin" comparisons.
  // This lets us allow navigations to/from data, error, or web bundle URLs
  // that originate from the same precursor URLs. Other rules may block these
  // navigations, but for the purpose of this throttle, these navigations are
  // valid.
  const url::SchemeHostPort& web_contents_isolation_tuple =
      web_contents_isolation_info->origin().GetTupleOrPrecursorTupleIfOpaque();
  const url::SchemeHostPort& dest_tuple =
      dest_origin_.GetTupleOrPrecursorTupleIfOpaque();
  DCHECK(web_contents_isolation_tuple.IsValid());

  // If the main frame tries to leave the app's origin, cancel the navigation
  // and open the URL in the user's default browser. Iframes are allowed to
  // leave the app's origin.
  if (dest_tuple != web_contents_isolation_tuple) {
    // TODO(crbug.com/1237636): Load the URL in the default browser.
    return navigation_handle()->IsInMainFrame() ? NavigationThrottle::CANCEL
                                                : NavigationThrottle::PROCEED;
  }

  // Block renderer-initiated iframe navigations into the app that were
  // initiated by a non-app frame. This ensures that all iframe navigations into
  // the app come from the app itself.
  absl::optional<url::SchemeHostPort> prev_tuple =
      GetTupleFromOptionalOrigin(prev_origin_);
  if (prev_tuple.has_value() &&
      prev_tuple.value() != web_contents_isolation_tuple &&
      navigation_handle()->IsRendererInitiated()) {
    // Main frames shouldn't have been allowed to leave the app's origin.
    CHECK(!navigation_handle()->IsInMainFrame());

    // Allow the navigation if it was initiated by the app, meaning it has a
    // trusted destination URL. This only applies to the initial request, as
    // redirect locations come from outside the app.
    if (navigation_handle()->GetRedirectChain().size() == 1 &&
        navigation_handle()->GetInitiatorOrigin().has_value()) {
      const url::SchemeHostPort& initiator_tuple =
          navigation_handle()
              ->GetInitiatorOrigin()
              .value()
              .GetTupleOrPrecursorTupleIfOpaque();
      if (initiator_tuple == web_contents_isolation_tuple) {
        return NavigationThrottle::PROCEED;
      }
    }
    return block_action;
  }

  // Block iframe navigations to the app's origin if the parent frame doesn't
  // belong to the app. This prevents non-app frames from having access to an
  // app frame.
  if (!navigation_handle()->IsInMainFrame()) {
    const url::SchemeHostPort& parent_tuple =
        navigation_handle()
            ->GetParentFrame()
            ->GetLastCommittedOrigin()
            .GetTupleOrPrecursorTupleIfOpaque();
    if (parent_tuple != web_contents_isolation_tuple) {
      return block_action;
    }
  }

  // At this point we know the navigation is same-tuple within an isolated app.
  // If the new page isn't isolated, block the navigation.
  if (!needs_app_isolation) {
    return block_action;
  }

  return NavigationThrottle::PROCEED;
}

bool IsolatedAppThrottle::embedder_requests_app_isolation() {
  BrowserContext* browser_context = NavigationRequest::From(navigation_handle())
                                        ->frame_tree_node()
                                        ->navigator()
                                        .controller()
                                        .GetBrowserContext();
  return GetContentClient()->browser()->ShouldUrlUseApplicationIsolationLevel(
      browser_context, navigation_handle()->GetURL());
}

const char* IsolatedAppThrottle::GetNameForLogging() {
  return "IsolatedAppThrottle";
}

}  // namespace content
