// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/prerender/prerender_navigation_throttle.h"

#include "content/browser/prerender/prerender_host.h"
#include "content/browser/prerender/prerender_host_registry.h"
#include "content/browser/prerender/prerender_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/public/browser/prerender_trigger_type.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

// Returns true if a the response code is disallowed for pre-rendering (e.g 404,
// etc), and false otherwise.
// TODO(crbug.com/1167592): This should be eventually synced with the outcome
// of https://github.com/jeremyroman/alternate-loading-modes/issues/30.
bool IsDisallowedHttpResponseCode(int response_code) {
  return response_code < 100 || response_code > 399;
}

// For the given two origins, analyze what kind of redirection happened.
void AnalyzeCrossOriginRedirection(
    const url::Origin& current_origin,
    const url::Origin& initial_origin,
    PrerenderTriggerType trigger_type,
    const std::string& embedder_histogram_suffix) {
  DCHECK_NE(initial_origin, current_origin);
  DCHECK_EQ(trigger_type, PrerenderTriggerType::kEmbedder);
  DCHECK(current_origin.GetURL().SchemeIsHTTPOrHTTPS());
  DCHECK(initial_origin.GetURL().SchemeIsHTTPOrHTTPS());

  std::bitset<3> bits;
  bits[2] = current_origin.scheme() != initial_origin.scheme();
  bits[1] = current_origin.host() != initial_origin.host();
  bits[0] = current_origin.port() != initial_origin.port();
  DCHECK(bits.any());
  auto mismatch_type =
      static_cast<PrerenderCrossOriginRedirectionMismatch>(bits.to_ulong());

  RecordPrerenderRedirectionMismatchType(mismatch_type, trigger_type,
                                         embedder_histogram_suffix);

  if (mismatch_type ==
      PrerenderCrossOriginRedirectionMismatch::kSchemePortMismatch) {
    RecordPrerenderRedirectionProtocolChange(
        current_origin.scheme() == url::kHttpsScheme
            ? PrerenderCrossOriginRedirectionProtocolChange::
                  kHttpProtocolUpgrade
            : PrerenderCrossOriginRedirectionProtocolChange::
                  kHttpProtocolDowngrade,
        trigger_type, embedder_histogram_suffix);
    return;
  }
  if (mismatch_type == PrerenderCrossOriginRedirectionMismatch::kHostMismatch) {
    if (current_origin.DomainIs(initial_origin.host())) {
      RecordPrerenderRedirectionDomain(
          PrerenderCrossOriginRedirectionDomain::kRedirectToSubDomain,
          trigger_type, embedder_histogram_suffix);
      return;
    }
    RecordPrerenderRedirectionDomain(
        initial_origin.DomainIs(current_origin.host())
            ? PrerenderCrossOriginRedirectionDomain::kRedirectFromSubDomain
            : PrerenderCrossOriginRedirectionDomain::kCrossDomain,
        trigger_type, embedder_histogram_suffix);
  }
}

}  // namespace

PrerenderNavigationThrottle::~PrerenderNavigationThrottle() = default;

// static
std::unique_ptr<PrerenderNavigationThrottle>
PrerenderNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  if (frame_tree_node->IsMainFrame() &&
      frame_tree_node->frame_tree()->is_prerendering()) {
    DCHECK(blink::features::IsPrerender2Enabled());
    return base::WrapUnique(new PrerenderNavigationThrottle(navigation_handle));
  }
  return nullptr;
}

const char* PrerenderNavigationThrottle::GetNameForLogging() {
  return "PrerenderNavigationThrottle";
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/false);
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillRedirectRequest() {
  return WillStartOrRedirectRequest(/*is_redirection=*/true);
}

PrerenderNavigationThrottle::PrerenderNavigationThrottle(
    NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {
  auto* navigation_request = NavigationRequest::From(navigation_handle);
  FrameTreeNode* ftn = navigation_request->frame_tree_node();
  PrerenderHostRegistry* prerender_host_registry =
      ftn->current_frame_host()->delegate()->GetPrerenderHostRegistry();
  int ftn_id = ftn->frame_tree_node_id();
  PrerenderHost* prerender_host =
      prerender_host_registry->FindNonReservedHostById(ftn_id);
  DCHECK(prerender_host);

  // This throttle is responsible for setting the initial navigation id on the
  // PrerenderHost, since the PrerenderHost obtains the NavigationRequest,
  // which has the ID, only after the navigation throttles run.
  if (prerender_host->GetInitialNavigationId().has_value()) {
    // If the host already has an initial navigation id, this throttle
    // will later cancel the navigation in Will*Request(). Just do nothing
    // until then.
  } else {
    prerender_host->SetInitialNavigation(
        static_cast<NavigationRequest*>(navigation_handle));
  }
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillStartOrRedirectRequest(bool is_redirection) {
  DCHECK(blink::features::IsPrerender2Enabled());

  // Take the root frame tree node of the prerendering page.
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
  DCHECK(frame_tree_node->IsMainFrame());
  DCHECK(frame_tree_node->frame_tree()->is_prerendering());

  // Get the prerender host of the prerendering page.
  PrerenderHostRegistry* prerender_host_registry =
      frame_tree_node->current_frame_host()
          ->delegate()
          ->GetPrerenderHostRegistry();
  PrerenderHost* prerender_host =
      prerender_host_registry->FindNonReservedHostById(
          frame_tree_node->frame_tree_node_id());
  DCHECK(prerender_host);

  // Navigations after the initial prerendering navigation are disallowed.
  if (*prerender_host->GetInitialNavigationId() !=
      navigation_request->GetNavigationId()) {
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        PrerenderHost::FinalStatus::kMainFrameNavigation);
    return CANCEL;
  }

  // Allow only HTTP(S) schemes.
  // https://wicg.github.io/nav-speculation/prerendering.html#no-bad-navs
  GURL prerendering_url = navigation_handle()->GetURL();
  if (!prerendering_url.SchemeIsHTTPOrHTTPS()) {
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderHost::FinalStatus::kInvalidSchemeRedirect
                       : PrerenderHost::FinalStatus::kInvalidSchemeNavigation);
    return CANCEL;
  }

  // TODO(https://crbug.com/1176120): Fallback to NoStatePrefetch.
  url::Origin prerendering_origin = url::Origin::Create(prerendering_url);
  if (prerender_host->IsBrowserInitiated()) {
    // Cancel an embedder triggered prerendering whenever redirected, this
    // redirection can be same-origin or cross-origin to the initial
    // prerendering URL.
    if (is_redirection) {
      url::Origin initial_origin =
          url::Origin::Create(prerender_host->GetInitialUrl());
      if (initial_origin == prerendering_origin) {
        prerender_host_registry->CancelHost(
            frame_tree_node->frame_tree_node_id(),
            PrerenderHost::FinalStatus::
                kEmbedderTriggeredAndSameOriginRedirected);
      } else {
        AnalyzeCrossOriginRedirection(
            prerendering_origin, initial_origin, prerender_host->trigger_type(),
            prerender_host->embedder_histogram_suffix());
        prerender_host_registry->CancelHost(
            frame_tree_node->frame_tree_node_id(),
            PrerenderHost::FinalStatus::
                kEmbedderTriggeredAndCrossOriginRedirected);
      }
      return CANCEL;
    }

    // Skip the same-origin check for non-redirected cases as the initiator
    // origin is nullopt for browser-initiated prerendering.
    DCHECK(!prerender_host->initiator_origin().has_value());
  } else if (prerendering_origin != prerender_host->initiator_origin()) {
    // Cancel prerendering if this is cross-origin prerendering, cross-origin
    // redirection during prerendering, or cross-origin navigation from a
    // prerendered page.
    prerender_host_registry->CancelHost(
        frame_tree_node->frame_tree_node_id(),
        is_redirection ? PrerenderHost::FinalStatus::kCrossOriginRedirect
                       : PrerenderHost::FinalStatus::kCrossOriginNavigation);
    return CANCEL;
  }

  return PROCEED;
}

NavigationThrottle::ThrottleCheckResult
PrerenderNavigationThrottle::WillProcessResponse() {
  auto* navigation_request = NavigationRequest::From(navigation_handle());
  absl::optional<PrerenderHost::FinalStatus> cancel_reason;

  if (navigation_handle()->IsDownload()) {
    // Disallow downloads during prerendering and cancel the prerender.
    cancel_reason = PrerenderHost::FinalStatus::kDownload;
  } else if (IsDisallowedHttpResponseCode(
                 navigation_request->commit_params().http_response_code)) {
    // There's no point in trying to prerender failed navigations.
    cancel_reason = PrerenderHost::FinalStatus::kNavigationBadHttpStatus;
  }

  if (cancel_reason.has_value()) {
    FrameTreeNode* frame_tree_node = navigation_request->frame_tree_node();
    DCHECK(frame_tree_node->frame_tree()->is_prerendering());

    PrerenderHostRegistry* prerender_host_registry =
        frame_tree_node->current_frame_host()
            ->delegate()
            ->GetPrerenderHostRegistry();

    prerender_host_registry->CancelHost(frame_tree_node->frame_tree_node_id(),
                                        cancel_reason.value());
    return CANCEL;
  }
  return PROCEED;
}

}  // namespace content
