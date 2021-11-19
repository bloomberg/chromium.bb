// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 *     (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/browser/renderer_host/navigation_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/optional_trace_event.h"
#include "base/trace_event/trace_conversion_helper.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_info.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_package/subresource_web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/common/content_constants_internal.h"
#include "content/common/navigation_params_utils.h"
#include "content/common/trace_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/replaced_navigation_entry_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "media/base/mime_util.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/history/session_history_constants.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/mojom/navigation/prefetched_signed_exchange_info.mojom.h"
#include "url/url_constants.h"

namespace content {
namespace {

// Invoked when entries have been pruned, or removed. For example, if the
// current entries are [google, digg, yahoo], with the current entry google,
// and the user types in cnet, then digg and yahoo are pruned.
void NotifyPrunedEntries(NavigationControllerImpl* nav_controller,
                         int index,
                         int count) {
  PrunedDetails details;
  details.index = index;
  details.count = count;
  nav_controller->delegate()->NotifyNavigationListPruned(details);
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
void ConfigureEntriesForRestore(
    std::vector<std::unique_ptr<NavigationEntryImpl>>* entries,
    RestoreType type) {
  for (auto& entry : *entries) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    entry->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    entry->set_restore_type(type);
  }
}

// Determines whether or not we should be carrying over a user agent override
// between two NavigationEntries.
bool ShouldKeepOverride(NavigationEntry* last_entry) {
  return last_entry && last_entry->GetIsOverridingUserAgent();
}

// Determines whether to override user agent for a navigation.
bool ShouldOverrideUserAgent(
    NavigationController::UserAgentOverrideOption override_user_agent,
    NavigationEntry* last_committed_entry) {
  switch (override_user_agent) {
    case NavigationController::UA_OVERRIDE_INHERIT:
      return ShouldKeepOverride(last_committed_entry);
    case NavigationController::UA_OVERRIDE_TRUE:
      return true;
    case NavigationController::UA_OVERRIDE_FALSE:
      return false;
  }
  NOTREACHED();
  return false;
}

// Returns true if this navigation should be treated as a reload. For e.g.
// navigating to the last committed url via the address bar or clicking on a
// link which results in a navigation to the last committed URL (but wasn't
// converted to do a replacement navigation in the renderer), etc.
// |node| is the FrameTreeNode which is navigating. |url|, |virtual_url|,
// |base_url_for_data_url|, |transition_type| correspond to the new navigation
// (i.e. the pending NavigationEntry). |last_committed_entry| is the last
// navigation that committed.
bool ShouldTreatNavigationAsReload(FrameTreeNode* node,
                                   const GURL& url,
                                   const GURL& virtual_url,
                                   const GURL& base_url_for_data_url,
                                   ui::PageTransition transition_type,
                                   bool is_post,
                                   bool should_replace_current_entry,
                                   NavigationEntryImpl* last_committed_entry) {
  // Navigations intended to do a replacement shouldn't be converted to do a
  // reload.
  if (should_replace_current_entry)
    return false;
  // Only convert to reload if at least one navigation committed.
  if (!last_committed_entry)
    return false;

  // Skip navigations initiated by external applications.
  if (transition_type & ui::PAGE_TRANSITION_FROM_API)
    return false;

  // We treat (PAGE_TRANSITION_RELOAD | PAGE_TRANSITION_FROM_ADDRESS_BAR),
  // PAGE_TRANSITION_TYPED or PAGE_TRANSITION_LINK transitions as navigations
  // which should be treated as reloads.
  bool transition_type_can_be_converted = false;
  if (ui::PageTransitionCoreTypeIs(transition_type,
                                   ui::PAGE_TRANSITION_RELOAD) &&
      (transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    transition_type_can_be_converted = true;
  }
  if (ui::PageTransitionCoreTypeIs(transition_type,
                                   ui::PAGE_TRANSITION_TYPED)) {
    transition_type_can_be_converted = true;
  }
  if (ui::PageTransitionCoreTypeIs(transition_type, ui::PAGE_TRANSITION_LINK))
    transition_type_can_be_converted = true;
  if (!transition_type_can_be_converted)
    return false;

  // This check is required for cases like view-source:, etc. Here the URL of
  // the navigation entry would contain the url of the page, while the virtual
  // URL contains the full URL including the view-source prefix.
  if (virtual_url != last_committed_entry->GetVirtualURL())
    return false;

  // Check that the URLs match.
  FrameNavigationEntry* frame_entry = last_committed_entry->GetFrameEntry(node);
  // If there's no frame entry then by definition the URLs don't match.
  if (!frame_entry)
    return false;

  if (url != frame_entry->url())
    return false;

  // This check is required for Android WebView loadDataWithBaseURL. Apps
  // can pass in anything in the base URL and we need to ensure that these
  // match before classifying it as a reload.
  if (url.SchemeIs(url::kDataScheme) && base_url_for_data_url.is_valid()) {
    if (base_url_for_data_url != last_committed_entry->GetBaseURLForDataURL())
      return false;
  }

  // Skip entries with SSL errors.
  if (last_committed_entry->ssl_error())
    return false;

  // Don't convert to a reload when the last navigation was a POST or the new
  // navigation is a POST.
  if (frame_entry->get_has_post_data() || is_post)
    return false;

  return true;
}

bool DoesURLMatchOriginForNavigation(
    const GURL& url,
    const absl::optional<url::Origin>& origin,
    SubresourceWebBundleNavigationInfo*
        subresource_web_bundle_navigation_info) {
  // If there is no origin supplied there is nothing to match. This can happen
  // for navigations to a pending entry and therefore it should be allowed.
  if (!origin)
    return true;

  // Urn: and uuid-in-package: subframe from WebBundle has an opaque origin
  // derived from the Bundle's origin.
  // TODO(https://crbug.com/1257045): Remove urn: scheme support.
  if ((url.SchemeIs(url::kUrnScheme) ||
       url.SchemeIs(url::kUuidInPackageScheme)) &&
      subresource_web_bundle_navigation_info) {
    return origin->CanBeDerivedFrom(
        subresource_web_bundle_navigation_info->bundle_url());
  }

  return origin->CanBeDerivedFrom(url);
}

absl::optional<url::Origin> GetCommittedOriginForFrameEntry(
    const mojom::DidCommitProvisionalLoadParams& params,
    NavigationRequest* request) {
  // Error pages commit in an opaque origin, yet have the real URL that resulted
  // in an error as the |params.url|. Since successful reload of an error page
  // should commit in the correct origin, setting the opaque origin on the
  // FrameNavigationEntry will be incorrect.
  if (request->DidEncounterError())
    return absl::nullopt;

  // We also currently don't save committed origins for loadDataWithBaseURL
  // navigations (probably accidentally). Without this check, navigations to
  // the FrameNavigationEntry might fail the DoesURLMatchOriginForNavigation()
  // check since the origin will be based on the base URL instead of the data:
  // URL used for the navigation.
  // TODO(https://crbug.com/1198406): Save committed origin in
  // FrameNavigationEntry for this case too.
  if (request->IsLoadDataWithBaseURL())
    return absl::nullopt;

  return absl::make_optional(params.origin);
}

bool IsValidURLForNavigation(bool is_main_frame,
                             const GURL& virtual_url,
                             const GURL& dest_url) {
  // Don't attempt to navigate if the virtual URL is non-empty and invalid.
  if (is_main_frame && !virtual_url.is_valid() && !virtual_url.is_empty()) {
    LOG(WARNING) << "Refusing to load for invalid virtual URL: "
                 << virtual_url.possibly_invalid_spec();
    return false;
  }

  // Don't attempt to navigate to non-empty invalid URLs.
  if (!dest_url.is_valid() && !dest_url.is_empty()) {
    LOG(WARNING) << "Refusing to load invalid URL: "
                 << dest_url.possibly_invalid_spec();
    return false;
  }

  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (dest_url.spec().size() > url::kMaxURLChars) {
    LOG(WARNING) << "Refusing to load URL as it exceeds " << url::kMaxURLChars
                 << " characters.";
    return false;
  }

  // Reject renderer debug URLs because they should have been handled before
  // we get to this point. This check handles renderer debug URLs
  // that are inside a view-source: URL (e.g. view-source:chrome://kill) and
  // provides defense-in-depth if a renderer debug URL manages to get here via
  // some other path. We want to reject the navigation here so it doesn't
  // violate assumptions in downstream code.
  if (blink::IsRendererDebugURL(dest_url)) {
    LOG(WARNING) << "Refusing to load renderer debug URL: "
                 << dest_url.possibly_invalid_spec();
    return false;
  }

  return true;
}

// See replaced_navigation_entry_data.h for details: this information is meant
// to ensure |*output_entry| keeps track of its original URL (landing page in
// case of server redirects) as it gets replaced (e.g. history.replaceState()),
// without overwriting it later, for main frames.
void CopyReplacedNavigationEntryDataIfPreviouslyEmpty(
    NavigationEntryImpl* replaced_entry,
    NavigationEntryImpl* output_entry) {
  if (output_entry->GetReplacedEntryData().has_value())
    return;

  ReplacedNavigationEntryData data;
  data.first_committed_url = replaced_entry->GetURL();
  data.first_timestamp = replaced_entry->GetTimestamp();
  data.first_transition_type = replaced_entry->GetTransitionType();
  output_entry->set_replaced_entry_data(data);
}

blink::mojom::NavigationType GetNavigationType(
    const GURL& old_url,
    const GURL& new_url,
    ReloadType reload_type,
    NavigationEntryImpl* entry,
    const FrameNavigationEntry& frame_entry,
    bool has_pending_cross_document_commit,
    bool is_currently_error_page,
    bool is_same_document_history_load) {
  // Reload navigations
  switch (reload_type) {
    case ReloadType::NORMAL:
      return blink::mojom::NavigationType::RELOAD;
    case ReloadType::BYPASSING_CACHE:
      return blink::mojom::NavigationType::RELOAD_BYPASSING_CACHE;
    case ReloadType::ORIGINAL_REQUEST_URL:
      return blink::mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL;
    case ReloadType::NONE:
      break;  // Fall through to rest of function.
  }

  if (entry->IsRestored()) {
    return entry->GetHasPostData()
               ? blink::mojom::NavigationType::RESTORE_WITH_POST
               : blink::mojom::NavigationType::RESTORE;
  }

  const bool can_be_same_document =
      // A pending cross-document commit means this navigation will not occur in
      // the current document, as that document would end up being replaced in
      // the meantime.
      !has_pending_cross_document_commit &&
      // If the current document is an error page, we should always treat it as
      // a different-document navigation so that we'll attempt to load the
      // document we're navigating to (and not stay in the current error page).
      !is_currently_error_page;

  // History navigations.
  if (frame_entry.page_state().IsValid()) {
    return can_be_same_document && is_same_document_history_load
               ? blink::mojom::NavigationType::HISTORY_SAME_DOCUMENT
               : blink::mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
  }
  DCHECK(!is_same_document_history_load);

  // A same-document fragment-navigation happens when the only part of the url
  // that is modified is after the '#' character.
  //
  // When modifying this condition, please take a look at:
  // FrameLoader::ShouldPerformFragmentNavigation().
  //
  // Note: this check is only valid for navigations that are not history
  // navigations. For instance, if the history is: 'A#bar' -> 'B' -> 'A#foo', a
  // history navigation from 'A#foo' to 'A#bar' is not a same-document
  // navigation, but a different-document one. This is why history navigation
  // are classified before this check.
  bool is_same_doc = new_url.has_ref() && old_url.EqualsIgnoringRef(new_url) &&
                     frame_entry.method() == "GET";

  // The one case where we do the wrong thing here and incorrectly choose
  // SAME_DOCUMENT is if the navigation is browser-initiated but the document in
  // the renderer is a frameset. All frameset navigations should be
  // DIFFERENT_DOCUMENT, even if their URLs match. A renderer-initiated
  // navigation would do the right thing, as it would send it to the browser and
  // all renderer-initiated navigations are DIFFERENT_DOCUMENT (they don't get
  // into this method). But since we can't tell that case here for browser-
  // initiated navigations, we have to get the renderer involved. In that case
  // the navigation would be restarted due to the renderer spending a reply of
  // mojom::CommitResult::RestartCrossDocument.

  return can_be_same_document && is_same_doc
             ? blink::mojom::NavigationType::SAME_DOCUMENT
             : blink::mojom::NavigationType::DIFFERENT_DOCUMENT;
}

// Adjusts the original input URL if needed, to get the URL to actually load and
// the virtual URL, which may differ.
void RewriteUrlForNavigation(const GURL& original_url,
                             BrowserContext* browser_context,
                             GURL* url_to_load,
                             GURL* virtual_url,
                             bool* reverse_on_redirect) {
  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  *url_to_load = *virtual_url = original_url;
  BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
      url_to_load, browser_context, reverse_on_redirect);
}

#if DCHECK_IS_ON()
// Helper sanity check function used in debug mode.
void ValidateRequestMatchesEntry(NavigationRequest* request,
                                 NavigationEntryImpl* entry) {
  if (request->frame_tree_node()->IsMainFrame()) {
    DCHECK_EQ(request->browser_initiated(), !entry->is_renderer_initiated());
    DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(
        ui::PageTransitionFromInt(request->common_params().transition),
        entry->GetTransitionType()));
  }
  DCHECK_EQ(request->commit_params().should_clear_history_list,
            entry->should_clear_history_list());
  DCHECK_EQ(request->common_params().has_user_gesture,
            entry->has_user_gesture());
  DCHECK_EQ(request->common_params().base_url_for_data_url,
            entry->GetBaseURLForDataURL());
  DCHECK_EQ(request->commit_params().can_load_local_resources,
            entry->GetCanLoadLocalResources());
  DCHECK_EQ(request->common_params().started_from_context_menu,
            entry->has_started_from_context_menu());

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(request->frame_tree_node());
  if (!frame_entry) {
    NOTREACHED();
    return;
  }

  DCHECK_EQ(request->common_params().method, frame_entry->method());

  size_t redirect_size = request->commit_params().redirects.size();
  if (redirect_size == frame_entry->redirect_chain().size()) {
    for (size_t i = 0; i < redirect_size; ++i) {
      DCHECK_EQ(request->commit_params().redirects[i],
                frame_entry->redirect_chain()[i]);
    }
  } else {
    NOTREACHED();
  }
}
#endif  // DCHECK_IS_ON()

// Returns whether the session history NavigationRequests in |navigations|
// would stay within the subtree of the sandboxed iframe in
// |sandbox_frame_tree_node_id|.
bool DoesSandboxNavigationStayWithinSubtree(
    int sandbox_frame_tree_node_id,
    const std::vector<std::unique_ptr<NavigationRequest>>& navigations) {
  for (auto& item : navigations) {
    bool within_subtree = false;
    // Check whether this NavigationRequest affects a frame within the
    // sandboxed frame's subtree by walking up the tree looking for the
    // sandboxed frame.
    for (auto* frame = item->frame_tree_node(); frame;
         frame = FrameTreeNode::From(frame->parent())) {
      if (frame->frame_tree_node_id() == sandbox_frame_tree_node_id) {
        within_subtree = true;
        break;
      }
    }
    if (!within_subtree)
      return false;
  }
  return true;
}

bool ShouldStorePolicyContainerPoliciesInFrameNavigationEntry(
    const NavigationRequest* request) {
  // For local schemes we need to store the policy container in the
  // FrameNavigationEntry, so that we can reload it in case of history
  // navigation.
  //
  // TODO(https://crbug.com/1146361 and https://crbug.com/1146362): blob: and
  // filesystem: should be removed from this list when we have properly
  // implemented storing their policy container in the respective store.
  return (request->common_params().url.SchemeIs(url::kAboutScheme) ||
          request->common_params().url.SchemeIs(url::kDataScheme) ||
          request->common_params().url.SchemeIsBlob() ||
          request->common_params().url.SchemeIsFileSystem());
}

}  // namespace

// NavigationControllerImpl::PendingEntryRef------------------------------------

NavigationControllerImpl::PendingEntryRef::PendingEntryRef(
    base::WeakPtr<NavigationControllerImpl> controller)
    : controller_(controller) {}

NavigationControllerImpl::PendingEntryRef::~PendingEntryRef() {
  if (!controller_)  // Can be null with interstitials.
    return;

  controller_->PendingEntryRefDeleted(this);
}

// NavigationControllerImpl ----------------------------------------------------

const size_t kMaxEntryCountForTestingNotSet = static_cast<size_t>(-1);

// static
size_t NavigationControllerImpl::max_entry_count_for_testing_ =
    kMaxEntryCountForTestingNotSet;

// Should Reload check for post data? The default is true, but is set to false
// when testing.
static bool g_check_for_repost = true;

// static
std::unique_ptr<NavigationEntry> NavigationController::CreateNavigationEntry(
    const GURL& url,
    Referrer referrer,
    absl::optional<url::Origin> initiator_origin,
    ui::PageTransition transition,
    bool is_renderer_initiated,
    const std::string& extra_headers,
    BrowserContext* browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  return NavigationControllerImpl::CreateNavigationEntry(
      url, referrer, std::move(initiator_origin),
      nullptr /* source_site_instance */, transition, is_renderer_initiated,
      extra_headers, browser_context, std::move(blob_url_loader_factory));
}

// static
std::unique_ptr<NavigationEntryImpl>
NavigationControllerImpl::CreateNavigationEntry(
    const GURL& url,
    Referrer referrer,
    absl::optional<url::Origin> initiator_origin,
    SiteInstance* source_site_instance,
    ui::PageTransition transition,
    bool is_renderer_initiated,
    const std::string& extra_headers,
    BrowserContext* browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  GURL url_to_load;
  GURL virtual_url;
  bool reverse_on_redirect = false;
  RewriteUrlForNavigation(url, browser_context, &url_to_load, &virtual_url,
                          &reverse_on_redirect);

  // Let the NTP override the navigation params and pretend that this is a
  // browser-initiated, bookmark-like navigation.
  GetContentClient()->browser()->OverrideNavigationParams(
      source_site_instance, &transition, &is_renderer_initiated, &referrer,
      &initiator_origin);

  auto entry = std::make_unique<NavigationEntryImpl>(
      nullptr,  // The site instance for tabs is sent on navigation
                // (WebContents::GetSiteInstance).
      url_to_load, referrer, initiator_origin, std::u16string(), transition,
      is_renderer_initiated, blob_url_loader_factory);
  entry->SetVirtualURL(virtual_url);
  entry->set_user_typed_url(virtual_url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  entry->set_extra_headers(extra_headers);
  return entry;
}

// static
void NavigationController::DisablePromptOnRepost() {
  g_check_for_repost = false;
}

base::Time NavigationControllerImpl::TimeSmoother::GetSmoothedTime(
    base::Time t) {
  // If |t| is between the water marks, we're in a run of duplicates
  // or just getting out of it, so increase the high-water mark to get
  // a time that probably hasn't been used before and return it.
  if (low_water_mark_ <= t && t <= high_water_mark_) {
    high_water_mark_ += base::Microseconds(1);
    return high_water_mark_;
  }

  // Otherwise, we're clear of the last duplicate run, so reset the
  // water marks.
  low_water_mark_ = high_water_mark_ = t;
  return t;
}

NavigationControllerImpl::ScopedShowRepostDialogForTesting::
    ScopedShowRepostDialogForTesting()
    : was_disallowed_(g_check_for_repost) {
  g_check_for_repost = true;
}

NavigationControllerImpl::ScopedShowRepostDialogForTesting::
    ~ScopedShowRepostDialogForTesting() {
  g_check_for_repost = was_disallowed_;
}

NavigationControllerImpl::NavigationControllerImpl(
    BrowserContext* browser_context,
    FrameTree& frame_tree,
    NavigationControllerDelegate* delegate)
    : frame_tree_(frame_tree),
      browser_context_(browser_context),
      delegate_(delegate),
      ssl_manager_(this),
      get_timestamp_callback_(base::BindRepeating(&base::Time::Now)) {
  DCHECK(browser_context_);
}

NavigationControllerImpl::~NavigationControllerImpl() {
  // The NavigationControllerImpl might be called inside its delegate
  // destructor. Calling it is not valid anymore.
  delegate_ = nullptr;
  DiscardNonCommittedEntries();
}

WebContents* NavigationControllerImpl::DeprecatedGetWebContents() {
  return delegate_->DeprecatedGetWebContents();
}

BrowserContext* NavigationControllerImpl::GetBrowserContext() {
  return browser_context_;
}

void NavigationControllerImpl::Restore(
    int selected_navigation,
    RestoreType type,
    std::vector<std::unique_ptr<NavigationEntry>>* entries) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK_EQ(0, GetEntryCount());
  DCHECK(!GetPendingEntry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(entries->size()));
  DCHECK_EQ(-1, pending_entry_index_);

  needs_reload_ = true;
  needs_reload_type_ = NeedsReloadType::kRestoreSession;
  entries_.reserve(entries->size());
  for (auto& entry : *entries)
    entries_.push_back(
        NavigationEntryImpl::FromNavigationEntry(std::move(entry)));

  // At this point, the |entries| is full of empty scoped_ptrs, so it can be
  // cleared out safely.
  entries->clear();

  // And finish the restore.
  FinishRestore(selected_navigation, type);
}

void NavigationControllerImpl::Reload(ReloadType reload_type,
                                      bool check_for_repost) {
  DCHECK_NE(ReloadType::NONE, reload_type);
  NavigationEntryImpl* entry = nullptr;
  int current_index = -1;

  if (entry_replaced_by_post_commit_error_) {
    // If there is an entry that was replaced by a currently active post-commit
    // error navigation, this can't be the initial navigation.
    DCHECK(!IsInitialNavigation());
    // If the current entry is a post commit error, we reload the entry it
    // replaced instead. We leave the error entry in place until a commit
    // replaces it, but the pending entry points to the original entry in the
    // meantime. Note that NavigateToExistingPendingEntry is able to handle the
    // case that pending_entry_ != entries_[pending_entry_index_].
    entry = entry_replaced_by_post_commit_error_.get();
    current_index = GetCurrentEntryIndex();
  } else if (IsInitialNavigation() && pending_entry_) {
    // If we are reloading the initial navigation, just use the current
    // pending entry.  Otherwise look up the current entry.
    entry = pending_entry_;
    // The pending entry might be in entries_ (e.g., after a Clone), so we
    // should also update the current_index.
    current_index = pending_entry_index_;
  } else {
    DiscardNonCommittedEntries();
    current_index = GetCurrentEntryIndex();
    if (current_index != -1) {
      entry = GetEntryAtIndex(current_index);
    }
  }

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (!entry)
    return;

  // Set ReloadType for |entry|.
  entry->set_reload_type(reload_type);

  if (g_check_for_repost && check_for_repost && entry->GetHasPostData()) {
    // The user is asking to reload a page with POST data. Prompt to make sure
    // they really want to do this. If they do, the dialog will call us back
    // with check_for_repost = false.
    delegate_->NotifyBeforeFormRepostWarningShow();

    pending_reload_ = reload_type;
    delegate_->ActivateAndShowRepostFormWarningDialog();
    return;
  }

  if (!IsInitialNavigation())
    DiscardNonCommittedEntries();

  pending_entry_ = entry;
  pending_entry_index_ = current_index;
  pending_entry_->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);

  // location.reload() goes through BeginNavigation, so all reloads triggered
  // via this codepath are browser initiated.
  NavigateToExistingPendingEntry(reload_type,
                                 FrameTreeNode::kFrameTreeNodeInvalidId,
                                 true /* is_browser_initiated */);
}

void NavigationControllerImpl::CancelPendingReload() {
  DCHECK(pending_reload_ != ReloadType::NONE);
  pending_reload_ = ReloadType::NONE;
}

void NavigationControllerImpl::ContinuePendingReload() {
  if (pending_reload_ == ReloadType::NONE) {
    NOTREACHED();
  } else {
    Reload(pending_reload_, false);
    pending_reload_ = ReloadType::NONE;
  }
}

bool NavigationControllerImpl::IsInitialNavigation() {
  return is_initial_navigation_;
}

bool NavigationControllerImpl::IsInitialBlankNavigation() {
  // TODO(creis): Once we create a NavigationEntry for the initial blank page,
  // we'll need to check for entry count 1 and restore_type NONE (to exclude
  // the cloned tab case).
  return IsInitialNavigation() && GetEntryCount() == 0;
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryWithUniqueID(
    int nav_entry_id) const {
  int index = GetEntryIndexWithUniqueID(nav_entry_id);
  return (index != -1) ? entries_[index].get() : nullptr;
}

NavigationEntryImpl*
NavigationControllerImpl::GetEntryWithUniqueIDIncludingPending(
    int nav_entry_id) const {
  NavigationEntryImpl* entry = GetEntryWithUniqueID(nav_entry_id);
  if (entry)
    return entry;
  return pending_entry_ && pending_entry_->GetUniqueID() == nav_entry_id
             ? pending_entry_
             : nullptr;
}

void NavigationControllerImpl::RegisterExistingOriginToPreventOptInIsolation(
    const url::Origin& origin) {
  for (int i = 0; i < GetEntryCount(); i++) {
    auto* entry = GetEntryAtIndex(i);
    entry->RegisterExistingOriginToPreventOptInIsolation(origin);
  }
  if (entry_replaced_by_post_commit_error_) {
    // It's possible we could come back to this entry if the error
    // page/interstitial goes away.
    entry_replaced_by_post_commit_error_
        ->RegisterExistingOriginToPreventOptInIsolation(origin);
  }
  // TODO(wjmaclean): Register pending commit NavigationRequests rather than
  // visiting pending_entry_, which lacks a committed origin. This will be done
  // in https://chromium-review.googlesource.com/c/chromium/src/+/2136703.
}

void NavigationControllerImpl::SetPendingEntry(
    std::unique_ptr<NavigationEntryImpl> entry) {
  DiscardNonCommittedEntries();
  pending_entry_ = entry.release();
  DCHECK_EQ(-1, pending_entry_index_);
}

NavigationEntryImpl* NavigationControllerImpl::GetActiveEntry() {
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

NavigationEntryImpl* NavigationControllerImpl::GetVisibleEntry() {
  // The pending entry is safe to return for new (non-history), browser-
  // initiated navigations.  Most renderer-initiated navigations should not
  // show the pending entry, to prevent URL spoof attacks.
  //
  // We make an exception for renderer-initiated navigations in new tabs, as
  // long as no other page has tried to access the initial empty document in
  // the new tab.  If another page modifies this blank page, a URL spoof is
  // possible, so we must stop showing the pending entry.
  bool safe_to_show_pending =
      pending_entry_ &&
      // Require a new navigation.
      pending_entry_index_ == -1 &&
      // Require either browser-initiated or an unmodified new tab.
      (!pending_entry_->is_renderer_initiated() || IsUnmodifiedBlankTab());

  // Also allow showing the pending entry for history navigations in a new tab,
  // such as Ctrl+Back.  In this case, no existing page is visible and no one
  // can script the new tab before it commits.
  if (!safe_to_show_pending && pending_entry_ && pending_entry_index_ != -1 &&
      IsInitialNavigation() && !pending_entry_->is_renderer_initiated())
    safe_to_show_pending = true;

  if (safe_to_show_pending)
    return pending_entry_;
  return GetLastCommittedEntry();
}

int NavigationControllerImpl::GetCurrentEntryIndex() {
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntryImpl* NavigationControllerImpl::GetLastCommittedEntry() {
  if (last_committed_entry_index_ == -1)
    return nullptr;
  return entries_[last_committed_entry_index_].get();
}

bool NavigationControllerImpl::CanViewSource() {
  const std::string& mime_type =
      frame_tree_.root()->current_frame_host()->GetPage().contents_mime_type();
  bool is_viewable_mime_type = blink::IsSupportedNonImageMimeType(mime_type) &&
                               !media::IsSupportedMediaMimeType(mime_type);
  NavigationEntry* visible_entry = GetVisibleEntry();
  return visible_entry && !visible_entry->IsViewSourceMode() &&
         is_viewable_mime_type;
}

int NavigationControllerImpl::GetLastCommittedEntryIndex() {
  // The last committed entry index must always be less than the number of
  // entries.  If there are no entries, it must be -1.
  DCHECK_LT(last_committed_entry_index_, GetEntryCount());
  DCHECK(GetEntryCount() || last_committed_entry_index_ == -1);
  return last_committed_entry_index_;
}

int NavigationControllerImpl::GetEntryCount() {
  DCHECK_LE(entries_.size(), max_entry_count());
  return static_cast<int>(entries_.size());
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtIndex(int index) {
  if (index < 0 || index >= GetEntryCount())
    return nullptr;

  return entries_[index].get();
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtOffset(int offset) {
  return GetEntryAtIndex(GetIndexForOffset(offset));
}

int NavigationControllerImpl::GetIndexForOffset(int offset) {
  return GetCurrentEntryIndex() + offset;
}

bool NavigationControllerImpl::CanGoBack() {
  for (int index = GetIndexForOffset(-1); index >= 0; index--) {
    if (!GetEntryAtIndex(index)->should_skip_on_back_forward_ui())
      return true;
  }
  return false;
}

bool NavigationControllerImpl::CanGoForward() {
  for (int index = GetIndexForOffset(1); index < GetEntryCount(); index++) {
    if (!GetEntryAtIndex(index)->should_skip_on_back_forward_ui())
      return true;
  }
  return false;
}

bool NavigationControllerImpl::CanGoToOffset(int offset) {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetEntryCount();
}

#if defined(OS_ANDROID)
bool NavigationControllerImpl::CanGoToOffsetWithSkipping(int offset) {
  if (offset == 0)
    return true;
  int increment = offset > 0 ? 1 : -1;
  int non_skippable_entries = 0;
  for (int index = GetIndexForOffset(increment);
       index >= 0 && index < GetEntryCount(); index += increment) {
    if (!GetEntryAtIndex(index)->should_skip_on_back_forward_ui())
      non_skippable_entries++;

    if (non_skippable_entries == std::abs(offset))
      return true;
  }
  return false;
}
#endif

void NavigationControllerImpl::GoBack() {
  int target_index = GetIndexForOffset(-1);

  // Move the target index past the skippable entries.
  bool all_skippable_entries = true;
  while (target_index >= 0) {
    if (!GetEntryAtIndex(target_index)->should_skip_on_back_forward_ui()) {
      all_skippable_entries = false;
      break;
    }
    target_index--;
  }

  // Do nothing if all entries are skippable. Normally this path would not
  // happen as consumers would have already checked it in CanGoBack but a lot of
  // tests do not do that.
  if (all_skippable_entries)
    return;

  GoToIndex(target_index);
}

void NavigationControllerImpl::GoForward() {
  int target_index = GetIndexForOffset(1);

  // Note that at least one entry (the last one) will be non-skippable since
  // entries are marked skippable only when they add another entry because of
  // redirect or pushState.
  while (target_index < static_cast<int>(entries_.size())) {
    if (!GetEntryAtIndex(target_index)->should_skip_on_back_forward_ui())
      break;
    target_index++;
  }
  GoToIndex(target_index);
}

void NavigationControllerImpl::GoToIndex(int index) {
  GoToIndex(index, FrameTreeNode::kFrameTreeNodeInvalidId,
            true /* is_browser_initiated */);
}

void NavigationControllerImpl::GoToIndex(int index,
                                         int sandbox_frame_tree_node_id,
                                         bool is_browser_initiated) {
  DCHECK(sandbox_frame_tree_node_id == FrameTreeNode::kFrameTreeNodeInvalidId ||
         !is_browser_initiated);
  TRACE_EVENT0("browser,navigation,benchmark",
               "NavigationControllerImpl::GoToIndex");
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

  DiscardNonCommittedEntries();

  DCHECK_EQ(nullptr, pending_entry_);
  DCHECK_EQ(-1, pending_entry_index_);
  pending_entry_ = entries_[index].get();
  pending_entry_index_ = index;
  pending_entry_->SetTransitionType(ui::PageTransitionFromInt(
      pending_entry_->GetTransitionType() | ui::PAGE_TRANSITION_FORWARD_BACK));
  NavigateToExistingPendingEntry(ReloadType::NONE, sandbox_frame_tree_node_id,
                                 is_browser_initiated);
}

void NavigationControllerImpl::GoToOffset(int offset) {
  // Note: This is actually reached in unit tests.
  if (!CanGoToOffset(offset))
    return;

  GoToIndex(GetIndexForOffset(offset));
}

void NavigationControllerImpl::GoToOffsetFromRenderer(int offset) {
  // Note: This is actually reached in unit tests.
  if (!CanGoToOffset(offset))
    return;

  GoToIndex(GetIndexForOffset(offset), FrameTreeNode::kFrameTreeNodeInvalidId,
            false /* is_browser_initiated */);
}

#if defined(OS_ANDROID)
void NavigationControllerImpl::GoToOffsetWithSkipping(int offset) {
  // Note: This is actually reached in unit tests.
  if (!CanGoToOffsetWithSkipping(offset))
    return;

  if (offset == 0) {
    GoToIndex(GetIndexForOffset(offset));
    return;
  }
  int increment = offset > 0 ? 1 : -1;
  // Find the offset without counting skippable entries.
  int target_index = GetIndexForOffset(increment);
  int non_skippable_entries = 0;
  for (int index = target_index; index >= 0 && index < GetEntryCount();
       index += increment) {
    if (!GetEntryAtIndex(index)->should_skip_on_back_forward_ui())
      non_skippable_entries++;

    if (non_skippable_entries == std::abs(offset)) {
      target_index = index;
      break;
    }
  }

  GoToIndex(target_index);
}
#endif

bool NavigationControllerImpl::RemoveEntryAtIndex(int index) {
  if (index == last_committed_entry_index_ || index == pending_entry_index_)
    return false;

  RemoveEntryAtIndexInternal(index);
  return true;
}

void NavigationControllerImpl::PruneForwardEntries() {
  DiscardNonCommittedEntries();
  int remove_start_index = last_committed_entry_index_ + 1;
  int num_removed = static_cast<int>(entries_.size()) - remove_start_index;
  if (num_removed <= 0)
    return;
  entries_.erase(entries_.begin() + remove_start_index, entries_.end());
  NotifyPrunedEntries(this, remove_start_index /* start index */,
                      num_removed /* count */);
}

void NavigationControllerImpl::UpdateVirtualURLToURL(NavigationEntryImpl* entry,
                                                     const GURL& new_url) {
  GURL new_virtual_url(new_url);
  if (BrowserURLHandlerImpl::GetInstance()->ReverseURLRewrite(
          &new_virtual_url, entry->GetVirtualURL(), browser_context_)) {
    entry->SetVirtualURL(new_virtual_url);
  }
}

base::WeakPtr<NavigationHandle> NavigationControllerImpl::LoadURL(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    const std::string& extra_headers) {
  LoadURLParams params(url);
  params.referrer = referrer;
  params.transition_type = transition;
  params.extra_headers = extra_headers;
  return LoadURLWithParams(params);
}

base::WeakPtr<NavigationHandle> NavigationControllerImpl::LoadURLWithParams(
    const LoadURLParams& params) {
  if (params.is_renderer_initiated)
    DCHECK(params.initiator_origin.has_value());

  TRACE_EVENT1("browser,navigation",
               "NavigationControllerImpl::LoadURLWithParams", "url",
               params.url.possibly_invalid_spec());
  bool is_explicit_navigation =
      GetContentClient()->browser()->IsExplicitNavigation(
          params.transition_type);
  if (HandleDebugURL(params.url, params.transition_type,
                     is_explicit_navigation)) {
    // If Telemetry is running, allow the URL load to proceed as if it's
    // unhandled, otherwise Telemetry can't tell if Navigation completed.
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            cc::switches::kEnableGpuBenchmarking))
      return nullptr;
  }

  // Checks based on params.load_type.
  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
    case LOAD_TYPE_HTTP_POST:
      break;
    case LOAD_TYPE_DATA:
      if (!params.url.SchemeIs(url::kDataScheme)) {
        NOTREACHED() << "Data load must use data scheme.";
        return nullptr;
      }
      break;
  }

  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  return NavigateWithoutEntry(params);
}

bool NavigationControllerImpl::PendingEntryMatchesRequest(
    NavigationRequest* request) const {
  return pending_entry_ &&
         pending_entry_->GetUniqueID() == request->nav_entry_id();
}

bool NavigationControllerImpl::RendererDidNavigate(
    RenderFrameHostImpl* rfh,
    const mojom::DidCommitProvisionalLoadParams& params,
    LoadCommittedDetails* details,
    bool is_same_document_navigation,
    bool was_on_initial_empty_document,
    bool previous_document_was_activated,
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  if (ShouldMaintainTrivialSessionHistory() && GetLastCommittedEntry()) {
    // Ensure that this navigation does not add a navigation entry, since
    // ShouldMaintainTrivialSessionHistory() means we should not add an entry
    // beyond the last committed one. Therefore, `should_replace_current_entry`
    // should be set, which replaces the current entry, or this should be a
    // reload, which does not create a new entry.
    DCHECK(params.should_replace_current_entry ||
           navigation_request->GetReloadType() != ReloadType::NONE);
  }
  is_initial_navigation_ = false;

  // Save the previous state before we clobber it.
  bool overriding_user_agent_changed = false;
  if (GetLastCommittedEntry()) {
    if (entry_replaced_by_post_commit_error_) {
      if (is_same_document_navigation) {
        // Same document navigations should not be possible on error pages and
        // would leave the controller in a weird state. Kill the renderer if
        // that happens.
        bad_message::ReceivedBadMessage(
            rfh->GetProcess(), bad_message::NC_SAME_DOCUMENT_POST_COMMIT_ERROR);
      }
      // Any commit while a post-commit error page is showing should put the
      // original entry back, replacing the error page's entry.  This includes
      // reloads, where the original entry was used as the pending entry and
      // should now be at the correct index at commit time.
      entries_[last_committed_entry_index_] =
          std::move(entry_replaced_by_post_commit_error_);
    }
    details->previous_main_frame_url = GetLastCommittedEntry()->GetURL();
    details->previous_entry_index = GetLastCommittedEntryIndex();
    if (pending_entry_ &&
        pending_entry_->GetIsOverridingUserAgent() !=
            GetLastCommittedEntry()->GetIsOverridingUserAgent())
      overriding_user_agent_changed = true;
#if defined(OS_ANDROID)
    // TODO(crbug.com/1266277): Clean up the logic of setting
    // |overriding_user_agent_changed| post-launch.
    if (base::FeatureList::IsEnabled(features::kRequestDesktopSiteGlobal)) {
      // Must honor user agent overrides in the |navigation_request|, such as
      // from things like RequestDesktopSiteWebContentsObserverAndroid. As a
      // result, besides comparing |pending_entry_|'s user agent against
      // LastCommittedEntry's, also need to compare |navigation_request|'s user
      // agent against LastCommittedEntry's.
      if (navigation_request->is_overriding_user_agent() !=
          GetLastCommittedEntry()->GetIsOverridingUserAgent()) {
        overriding_user_agent_changed = true;
      }
    }
#endif  // defined(OS_ANDROID)
  } else {
    // GetLastCommittedEntry() is null, so this is the first entry.
    details->previous_main_frame_url = GURL();
    details->previous_entry_index = -1;
    if (pending_entry_ && pending_entry_->GetIsOverridingUserAgent()) {
      // Default setting is NOT override the user agent, so overriding the user
      // agent in first entry should be considered as user agent changed as
      // well.
      overriding_user_agent_changed = true;
    }
#if defined(OS_ANDROID)
    // TODO(crbug.com/1266277): Clean up the logic of setting
    // |overriding_user_agent_changed| post-launch.
    if (base::FeatureList::IsEnabled(features::kRequestDesktopSiteGlobal)) {
      // Must honor user agent overrides in the |navigation_request|, such as
      // from things like RequestDesktopSiteWebContentsObserverAndroid. As a
      // result, besides checking |pending_entry_|'s user agent, also need to
      // check |navigation_request|'s.
      if (navigation_request->is_overriding_user_agent()) {
        overriding_user_agent_changed = true;
      }
    }
#endif  // defined(OS_ANDROID)
  }

  bool is_main_frame_navigation = !rfh->GetParent();

  // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
  // implementing back-forward cache.
  // For primary frame tree navigations, choose an appropriate
  // BackForwardCacheMetrics to be associated with the NavigationEntry, by
  // either creating a new object or reusing the previous one depending on
  // whether it's a main frame navigation or not.
  scoped_refptr<BackForwardCacheMetrics> back_forward_cache_metrics;
  if (navigation_request->frame_tree_node()->frame_tree()->type() ==
      FrameTree::Type::kPrimary) {
    back_forward_cache_metrics =
        BackForwardCacheMetrics::CreateOrReuseBackForwardCacheMetrics(
            GetLastCommittedEntry(), is_main_frame_navigation,
            params.document_sequence_number);
  }
  // Notify the last active entry that we have navigated away.
  if (is_main_frame_navigation && !is_same_document_navigation) {
    if (NavigationEntryImpl* navigation_entry = GetLastCommittedEntry()) {
      if (auto* metrics = navigation_entry->back_forward_cache_metrics()) {
        metrics->MainFrameDidNavigateAwayFromDocument(rfh, navigation_request);
      }
    }
  }

  // If there is a pending entry at this point, it should have a SiteInstance,
  // except for restored entries.
  bool was_restored = false;
  DCHECK(pending_entry_index_ == -1 || pending_entry_->site_instance() ||
         pending_entry_->IsRestored());
  if (pending_entry_ && pending_entry_->IsRestored()) {
    pending_entry_->set_restore_type(RestoreType::kNotRestored);
    was_restored = true;
  }

  // If this is a navigation to a matching pending_entry_ and the SiteInstance
  // has changed, this must be treated as a new navigation with replacement.
  // Set the replacement bit here and ClassifyNavigation will identify this
  // case and return NEW_ENTRY.
  if (!rfh->GetParent() && pending_entry_ &&
      pending_entry_->GetUniqueID() ==
          navigation_request->commit_params().nav_entry_id &&
      pending_entry_->site_instance() &&
      pending_entry_->site_instance() != rfh->GetSiteInstance()) {
    DCHECK_NE(-1, pending_entry_index_);
    // TODO(nasko,creis): Instead of setting this value here, set
    // should_replace_current_entry on the parameters we send to the
    // renderer process as part of CommitNavigation. The renderer should
    // in turn send it back here as part of |params| and it can be just
    // enforced and renderer process terminated on mismatch.
    details->did_replace_entry = true;
  } else {
    // The renderer tells us whether the navigation replaces the current entry.
    details->did_replace_entry = params.should_replace_current_entry;
  }

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(rfh, params, navigation_request);

  // is_same_document must be computed before the entry gets committed.
  details->is_same_document = is_same_document_navigation;

  details->is_prerender_activation =
      navigation_request->IsPrerenderedPageActivation();

  // Make sure we do not discard the pending entry for a different ongoing
  // navigation when a same document commit comes in unexpectedly from the
  // renderer.  Limit this to a very narrow set of conditions to avoid risks to
  // other navigation types. See https://crbug.com/900036.
  // TODO(crbug.com/926009): Handle history.pushState() as well.
  bool keep_pending_entry = is_same_document_navigation &&
                            details->type == NAVIGATION_TYPE_EXISTING_ENTRY &&
                            pending_entry_ &&
                            !PendingEntryMatchesRequest(navigation_request);

  switch (details->type) {
    case NAVIGATION_TYPE_NEW_ENTRY:
      RendererDidNavigateToNewEntry(
          rfh, params, details->is_same_document, details->did_replace_entry,
          previous_document_was_activated, navigation_request);
      break;
    case NAVIGATION_TYPE_EXISTING_ENTRY:
      RendererDidNavigateToExistingEntry(rfh, params, details->is_same_document,
                                         was_restored, navigation_request,
                                         keep_pending_entry);
      break;
    case NAVIGATION_TYPE_NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(
          rfh, params, details->is_same_document, details->did_replace_entry,
          previous_document_was_activated, navigation_request);
      break;
    case NAVIGATION_TYPE_AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(
              rfh, params, details->is_same_document,
              was_on_initial_empty_document, navigation_request)) {
        // We don't send a notification about auto-subframe PageState during
        // UpdateStateForFrame, since it looks like nothing has changed.  Send
        // it here at commit time instead.
        NotifyEntryChanged(GetLastCommittedEntry());
        return false;
      }
      break;
    case NAVIGATION_TYPE_NAV_IGNORE:
      // If a pending navigation was in progress, this canceled it.  We should
      // discard it and make sure it is removed from the URL bar.  After that,
      // there is nothing we can do with this navigation, so we just return to
      // the caller that nothing has happened.
      if (pending_entry_)
        DiscardNonCommittedEntries();
      return false;
    case NAVIGATION_TYPE_UNKNOWN:
      NOTREACHED();
      break;
  }

  // At this point, we know that the navigation has just completed, so
  // record the time.
  //
  // TODO(akalin): Use "sane time" as described in
  // https://www.chromium.org/developers/design-documents/sane-time .
  base::Time timestamp =
      time_smoother_.GetSmoothedTime(get_timestamp_callback_.Run());
  DVLOG(1) << "Navigation finished at (smoothed) timestamp "
           << timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds();

  // If we aren't keeping the pending entry, there shouldn't be one at this
  // point. Clear it again in case any error cases above forgot to do so.
  // TODO(pbos): Consider a CHECK here that verifies that the pending entry has
  // been cleared instead of protecting against it.
  if (!keep_pending_entry)
    DiscardNonCommittedEntries();

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  DCHECK(params.page_state.IsValid()) << "Shouldn't see an empty PageState.";
  NavigationEntryImpl* active_entry = GetLastCommittedEntry();
  active_entry->SetTimestamp(timestamp);
  active_entry->SetHttpStatusCode(params.http_status_code);
  // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
  // implementing back-forward cache.
  if (back_forward_cache_metrics &&
      !active_entry->back_forward_cache_metrics()) {
    active_entry->set_back_forward_cache_metrics(
        std::move(back_forward_cache_metrics));
  }

  // `back_forward_cache_metrics()` may return null as we do not record
  // back-forward cache metrics for navigations in non-primary frame trees.
  if (active_entry->back_forward_cache_metrics()) {
    active_entry->back_forward_cache_metrics()->DidCommitNavigation(
        navigation_request,
        back_forward_cache_.IsAllowed(navigation_request->GetURL()));
  }

  // Grab the corresponding FrameNavigationEntry for a few updates, but only if
  // the SiteInstance matches (to avoid updating the wrong entry by mistake).
  // A mismatch can occur if the renderer lies or due to a unique name collision
  // after a race with an OOPIF (see https://crbug.com/616820).
  FrameNavigationEntry* frame_entry =
      active_entry->GetFrameEntry(rfh->frame_tree_node());
  if (frame_entry && frame_entry->site_instance() != rfh->GetSiteInstance())
    frame_entry = nullptr;
  // Make sure we've updated the PageState in one of the helper methods.
  // TODO(creis): Remove the "if" once https://crbug.com/522193 is fixed.
  if (frame_entry) {
    DCHECK(params.page_state == frame_entry->page_state());

    // Remember the bindings the renderer process has at this point, so that
    // we do not grant this entry additional bindings if we come back to it.
    frame_entry->SetBindings(rfh->GetEnabledBindings());
  }

  // Once it is committed, we no longer need to track several pieces of state on
  // the entry.
  active_entry->ResetForCommit(frame_entry);

  // The active entry's SiteInstance should match our SiteInstance.
  // TODO(creis): This check won't pass for subframes until we create entries
  // for subframe navigations.
  if (!rfh->GetParent())
    CHECK_EQ(active_entry->site_instance(), rfh->GetSiteInstance());

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = active_entry;
  details->is_main_frame = !rfh->GetParent();
  details->http_status_code = params.http_status_code;

  active_entry->SetIsOverridingUserAgent(
      navigation_request->is_overriding_user_agent());

  NotifyNavigationEntryCommitted(details);

  if (active_entry->GetURL().SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
      !navigation_request->DidEncounterError()) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.SecureSchemeHasSSLStatus",
                          !!active_entry->GetSSL().certificate);
  }

  if (overriding_user_agent_changed)
    delegate_->UpdateOverridingUserAgent();

  // Update the nav_entry_id for each RenderFrameHost in the tree, so that each
  // one knows the latest NavigationEntry it is showing (whether it has
  // committed anything in this navigation or not). This allows things like
  // state and title updates from RenderFrames to apply to the latest relevant
  // NavigationEntry.
  int nav_entry_id = active_entry->GetUniqueID();
  for (FrameTreeNode* node : frame_tree_.Nodes())
    node->current_frame_host()->set_nav_entry_id(nav_entry_id);

  if (navigation_request->IsPrerenderedPageActivation()) {
    BroadcastHistoryOffsetAndLength();
    // TODO(crbug.com/1222893): Broadcasting happens after the prerendered page
    // is activated. As a result, a "prerenderingchange" event listener sees the
    // history.length which is not updated yet. We should guarantee that
    // history's length and offset should be updated before a
    // "prerenderingchange" event listener runs. One possible approach is to use
    // the same IPC which "prerenderingchange" uses, and propagate history's
    // length and offset together with that.
  }

  return true;
}

NavigationType NavigationControllerImpl::ClassifyNavigation(
    RenderFrameHostImpl* rfh,
    const mojom::DidCommitProvisionalLoadParams& params,
    NavigationRequest* navigation_request) {
  TraceReturnReason<tracing_category::kNavigation> trace_return(
      "ClassifyNavigation");

  if (params.did_create_new_entry) {
    // A new entry. We may or may not have a corresponding pending entry, and
    // this may or may not be the main frame.
    if (!rfh->GetParent()) {
      trace_return.set_return_reason("new entry, no parent, new entry");
      return NAVIGATION_TYPE_NEW_ENTRY;
    }

    // When this is a new subframe navigation, we should have a committed page
    // in which it's a subframe. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry()) {
      trace_return.set_return_reason("new entry, no last committed, ignore");
      return NAVIGATION_TYPE_NAV_IGNORE;
    }

    // Valid subframe navigation.
    trace_return.set_return_reason("new entry, new subframe");
    return NAVIGATION_TYPE_NEW_SUBFRAME;
  }

  // We only clear the session history in tests when navigating to a new entry.
  DCHECK(!params.history_list_was_cleared);

  if (rfh->GetParent()) {
    // All manual subframes would be did_create_new_entry and handled above, so
    // we know this is auto.
    if (GetLastCommittedEntry()) {
      trace_return.set_return_reason("subframe, last commmited, auto subframe");
      return NAVIGATION_TYPE_AUTO_SUBFRAME;
    }

    // We ignore subframes created in non-committed pages; we'd appreciate if
    // people stopped doing that.
    trace_return.set_return_reason("subframe, no last commmited, ignore");
    return NAVIGATION_TYPE_NAV_IGNORE;
  }

  const int nav_entry_id = navigation_request->commit_params().nav_entry_id;
  if (nav_entry_id == 0) {
    // This is a renderer-initiated navigation (nav_entry_id == 0), but didn't
    // create a new page.

    // Just like above in the did_create_new_entry case, it's possible to
    // scribble onto an uncommitted page. Again, there isn't any navigation
    // stuff that we can do, so ignore it here as well.
    NavigationEntry* last_committed = GetLastCommittedEntry();
    if (!last_committed) {
      trace_return.set_return_reason("nav entry 0, no last committed, ignore");
      return NAVIGATION_TYPE_NAV_IGNORE;
    }

    // This main frame navigation is not a history navigation (since
    // nav_entry_id is 0), but didn't create a new entry. So this must be a
    // reload or a replacement navigation, which will modify the existing entry.
    //
    // TODO(nasko): With error page isolation, reloading an existing session
    // history entry can result in change of SiteInstance. Check for such a case
    // here and classify it as NEW_ENTRY, as such navigations should be treated
    // as new with replacement.
    trace_return.set_return_reason(
        "nav entry 0, last committed, existing entry");
    return NAVIGATION_TYPE_EXISTING_ENTRY;
  }

  if (pending_entry_ && pending_entry_->GetUniqueID() == nav_entry_id) {
    // If the SiteInstance of the |pending_entry_| does not match the
    // SiteInstance that got committed, treat this as a new navigation with
    // replacement. This can happen if back/forward/reload encounters a server
    // redirect to a different site or an isolated error page gets successfully
    // reloaded into a different SiteInstance.
    if (pending_entry_->site_instance() &&
        pending_entry_->site_instance() != rfh->GetSiteInstance()) {
      trace_return.set_return_reason("pending matching nav entry, new entry");
      return NAVIGATION_TYPE_NEW_ENTRY;
    }

    if (pending_entry_index_ == -1) {
      // In this case, we have a pending entry for a load of a new URL but Blink
      // didn't do a new navigation (params.did_create_new_entry). First check
      // to make sure Blink didn't treat a new cross-process navigation as
      // inert, and thus set params.did_create_new_entry to false. In that case,
      // we must treat it as NEW rather than the converted reload case below,
      // since the new SiteInstance doesn't match the last committed entry.
      if (!GetLastCommittedEntry() ||
          GetLastCommittedEntry()->site_instance() != rfh->GetSiteInstance()) {
        trace_return.set_return_reason("new pending, new entry");
        return NAVIGATION_TYPE_NEW_ENTRY;
      }

      // Otherwise, this happens when you press enter in the URL bar to reload.
      // We will create a pending entry, but NavigateWithoutEntry will convert
      // it to a reload since it's the same page and not create a new entry for
      // it. (The user doesn't want to have a new back/forward entry when they
      // do this.) Therefore we want to just ignore the pending entry and go
      // back to where we were (the "existing entry").
      trace_return.set_return_reason("new pending, existing (same) entry");
      return NAVIGATION_TYPE_EXISTING_ENTRY;
    }
  }

  // Everything below here is assumed to be an existing entry, but if there is
  // no last committed entry, we must consider it a new navigation instead.
  if (!GetLastCommittedEntry()) {
    trace_return.set_return_reason("no last committed, new entry");
    return NAVIGATION_TYPE_NEW_ENTRY;
  }

  if (navigation_request->commit_params().intended_as_new_entry) {
    // This was intended to be a navigation to a new entry but the pending entry
    // got cleared in the meanwhile. Classify as EXISTING_ENTRY because we may
    // or may not have a pending entry.
    trace_return.set_return_reason("intended as new entry, existing entry");
    return NAVIGATION_TYPE_EXISTING_ENTRY;
  }

  if (navigation_request->DidEncounterError() &&
      failed_pending_entry_id_ != 0 &&
      nav_entry_id == failed_pending_entry_id_) {
    // If the renderer was going to a new pending entry that got cleared because
    // of an error, this is the case of the user trying to retry a failed load
    // by pressing return. Classify as EXISTING_ENTRY because we probably don't
    // have a pending entry.
    trace_return.set_return_reason(
        "unreachable, matching pending, existing entry");
    return NAVIGATION_TYPE_EXISTING_ENTRY;
  }

  // Now we know that the notification is for an existing entry; find it.
  int existing_entry_index = GetEntryIndexWithUniqueID(nav_entry_id);
  trace_return.traced_value()->SetInteger("existing_entry_index",
                                          existing_entry_index);
  if (existing_entry_index == -1) {
    // The renderer has committed a navigation to an entry that no longer
    // exists. Because the renderer is showing that page, resurrect that entry.
    trace_return.set_return_reason("existing entry -1, new entry");
    return NAVIGATION_TYPE_NEW_ENTRY;
  }

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  trace_return.set_return_reason("default return, existing entry");
  return NAVIGATION_TYPE_EXISTING_ENTRY;
}

void NavigationControllerImpl::UpdateNavigationEntryDetails(
    NavigationEntryImpl* entry,
    RenderFrameHostImpl* rfh,
    const mojom::DidCommitProvisionalLoadParams& params,
    NavigationRequest* request,
    NavigationEntryImpl::UpdatePolicy update_policy,
    bool is_new_entry) {
  // Update the FrameNavigationEntry.
  entry->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), update_policy, params.item_sequence_number,
      params.document_sequence_number, params.app_history_key,
      rfh->GetSiteInstance(), nullptr, params.url,
      GetCommittedOriginForFrameEntry(params, request),
      Referrer(*params.referrer), request->common_params().initiator_origin,
      request->GetRedirectChain(), params.page_state, params.method,
      params.post_id, nullptr /* blob_url_loader_factory */,
      request->web_bundle_navigation_info()
          ? request->web_bundle_navigation_info()->Clone()
          : nullptr,
      request->GetSubresourceWebBundleNavigationInfo(),
      ComputePolicyContainerPoliciesForFrameEntry(
          rfh, request->IsSameDocument(), request));

  if (rfh->GetParent()) {
    // Only modify the NavigationEntry for main frame navigations.
    return;
  }
  if (entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(entry, params.url);
  // Don't use the page type from the pending entry. Some interstitial page
  // may have set the type to interstitial. Once we commit, however, the page
  // type must always be normal or error.
  entry->set_page_type(request->DidEncounterError() ? PAGE_TYPE_ERROR
                                                    : PAGE_TYPE_NORMAL);
  if (is_new_entry) {
    // Some properties of the NavigationEntry are only set when the entry is
    // new (we aren't reusing it).
    entry->SetTransitionType(params.transition);
    entry->SetOriginalRequestURL(request->GetOriginalRequestURL());
    DCHECK_EQ(rfh->is_overriding_user_agent(), params.is_overriding_user_agent);
    entry->SetIsOverridingUserAgent(params.is_overriding_user_agent);
  } else {
    // We are reusing the NavigationEntry. The site instance will normally be
    // the same except for a few cases:
    // 1) session restore, when no site instance will be assigned or
    // 2) redirect, when the site instance is reset.
    DCHECK(!entry->site_instance() || !entry->GetRedirectChain().empty() ||
           entry->site_instance() == rfh->GetSiteInstance());
  }
}

void NavigationControllerImpl::RendererDidNavigateToNewEntry(
    RenderFrameHostImpl* rfh,
    const mojom::DidCommitProvisionalLoadParams& params,
    bool is_same_document,
    bool replace_entry,
    bool previous_document_was_activated,
    NavigationRequest* request) {
  std::unique_ptr<NavigationEntryImpl> new_entry;
  const absl::optional<url::Origin>& initiator_origin =
      request->common_params().initiator_origin;

  // First check if this is an in-page navigation.  If so, clone the current
  // entry instead of looking at the pending entry, because the pending entry
  // does not have any subframe history items.
  if (is_same_document && GetLastCommittedEntry()) {
    auto frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
        rfh->frame_tree_node()->unique_name(), params.item_sequence_number,
        params.document_sequence_number, params.app_history_key,
        rfh->GetSiteInstance(), nullptr, params.url,
        GetCommittedOriginForFrameEntry(params, request),
        Referrer(*params.referrer), initiator_origin,
        request->GetRedirectChain(), params.page_state, params.method,
        params.post_id, nullptr /* blob_url_loader_factory */,
        nullptr /* web_bundle_navigation_info */,
        request->GetSubresourceWebBundleNavigationInfo(),
        // We will set the document policies later in this function.
        nullptr /* policy_container_policies */);

    new_entry = GetLastCommittedEntry()->CloneAndReplace(
        frame_entry, true, request->frame_tree_node(), frame_tree_.root());
    if (new_entry->GetURL().DeprecatedGetOriginAsURL() !=
        params.url.DeprecatedGetOriginAsURL()) {
      // TODO(jam): we had one report of this with a URL that was redirecting to
      // only tildes. Until we understand that better, don't copy the cert in
      // this case.
      new_entry->GetSSL() = SSLStatus();

      if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
          !request->DidEncounterError()) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus.NewPageInPageOriginMismatch",
            !!new_entry->GetSSL().certificate);
      }
    }

    // It is expected that |frame_entry| is now owned by |new_entry|. This means
    // that |frame_entry| should now have a reference count of exactly 2: one
    // due to the local variable |frame_entry|, and another due to |new_entry|
    // also retaining one. This should never fail, because it's the main frame.
    CHECK(!frame_entry->HasOneRef() && frame_entry->HasAtLeastOneRef());

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        !request->DidEncounterError()) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.SecureSchemeHasSSLStatus.NewPageInPage",
                            !!new_entry->GetSSL().certificate);
    }
  }

  // If this is an activation navigation from a prerendered page, transfer the
  // new entry from an entry already created and stored in the
  // NavigationRequest. |new_entry| will not have been set prior to this as
  // |is_same_document| is mutually exclusive with
  // |IsPrerenderedPageActivation|.
  if (request->IsPrerenderedPageActivation()) {
    DCHECK(!is_same_document);
    DCHECK(!new_entry);
    new_entry = request->TakePrerenderNavigationEntry();
    DCHECK(new_entry);
  }

  // Only make a copy of the pending entry if it is appropriate for the new
  // document that just loaded. Verify this by checking if the entry corresponds
  // to the given NavigationRequest. Additionally, coarsely check that:
  // 1. The SiteInstance hasn't been assigned to something else.
  // 2. The pending entry was intended as a new entry, rather than being a
  // history navigation that was interrupted by an unrelated,
  // renderer-initiated navigation.
  // TODO(csharrison): Investigate whether we can remove some of the coarser
  // checks.
  if (!new_entry && PendingEntryMatchesRequest(request) &&
      pending_entry_index_ == -1 &&
      (!pending_entry_->site_instance() ||
       pending_entry_->site_instance() == rfh->GetSiteInstance())) {
    new_entry = pending_entry_->Clone();

    new_entry->GetSSL() =
        SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        !request->DidEncounterError()) {
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.SecureSchemeHasSSLStatus.NewPagePendingEntryMatches",
          !!new_entry->GetSSL().certificate);
    }
  }

  // For cross-document commits with no matching pending entry, create a new
  // entry.
  if (!new_entry) {
    new_entry = std::make_unique<NavigationEntryImpl>(
        rfh->GetSiteInstance(), params.url, Referrer(*params.referrer),
        initiator_origin,
        std::u16string(),  // title
        params.transition, request->IsRendererInitiated(),
        nullptr);  // blob_url_loader_factory

    // Find out whether the new entry needs to update its virtual URL on URL
    // change and set up the entry accordingly. This is needed to correctly
    // update the virtual URL when replaceState is called after a pushState.
    GURL url = params.url;
    bool needs_update = false;
    // When navigating to a new entry, give the browser URL handler a chance to
    // update the virtual URL based on the new URL. For example, this is needed
    // to show chrome://bookmarks/#1 when the bookmarks webui extension changes
    // the URL.
    BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
        &url, browser_context_, &needs_update);
    new_entry->set_update_virtual_url_with_url(needs_update);

    new_entry->GetSSL() =
        SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        !request->DidEncounterError()) {
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.SecureSchemeHasSSLStatus.NewPageNoMatchingEntry",
          !!new_entry->GetSSL().certificate);
    }
  }

  // TODO(crbug.com/1179428) - determine which parts of the entry need to be set
  // for prerendered contents, if any. This is because prerendering/activation
  // technically won't be creating a new document. Unlike BFCache, prerendering
  // creates a new NavigationEntry rather than using an existing one.
  if (!request->IsPrerenderedPageActivation()) {
    UpdateNavigationEntryDetails(new_entry.get(), rfh, params, request,
                                 NavigationEntryImpl::UpdatePolicy::kUpdate,
                                 true /* is_new_entry */);

    // history.pushState() is classified as a navigation to a new page, but sets
    // is_same_document to true. In this case, we already have the title and
    // favicon available, so set them immediately.
    if (is_same_document && GetLastCommittedEntry()) {
      new_entry->SetTitle(GetLastCommittedEntry()->GetTitle());
      new_entry->GetFavicon() = GetLastCommittedEntry()->GetFavicon();
    }
  }

  DCHECK(!params.history_list_was_cleared || !replace_entry);
  // The browser requested to clear the session history when it initiated the
  // navigation. Now we know that the renderer has updated its state accordingly
  // and it is safe to also clear the browser side history.
  if (params.history_list_was_cleared) {
    DiscardNonCommittedEntries();
    entries_.clear();
    last_committed_entry_index_ = -1;
  }

  // If this is a new navigation with replacement and there is a
  // pending_entry_ which matches the navigation reported by the renderer
  // process, then it should be the one replaced, so update the
  // last_committed_entry_index_ to use it.
  if (replace_entry && pending_entry_index_ != -1 &&
      pending_entry_->GetUniqueID() == request->commit_params().nav_entry_id) {
    last_committed_entry_index_ = pending_entry_index_;
  }

  SetShouldSkipOnBackForwardUIIfNeeded(
      replace_entry, previous_document_was_activated,
      request->IsRendererInitiated(), request->GetPreviousPageUkmSourceId());

  InsertOrReplaceEntry(std::move(new_entry), replace_entry,
                       !request->post_commit_error_page_html().empty());
}

void NavigationControllerImpl::RendererDidNavigateToExistingEntry(
    RenderFrameHostImpl* rfh,
    const mojom::DidCommitProvisionalLoadParams& params,
    bool is_same_document,
    bool was_restored,
    NavigationRequest* request,
    bool keep_pending_entry) {
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";

  // We should only get here for main frame navigations.
  DCHECK(!rfh->GetParent());

  NavigationEntryImpl* entry = nullptr;
  if (request->commit_params().intended_as_new_entry) {
    // We're guaranteed to have a last committed entry if intended_as_new_entry
    // is true.
    entry = GetLastCommittedEntry();
    DCHECK(entry);

    // If the NavigationRequest matches a new pending entry and is classified as
    // EXISTING_ENTRY, then it is a navigation to the same URL that was
    // converted to a reload, such as a user pressing enter in the omnibox.
    if (pending_entry_ && pending_entry_index_ == -1 &&
        pending_entry_->GetUniqueID() ==
            request->commit_params().nav_entry_id) {
      // Note: The pending entry will usually have a real ReloadType here, but
      // it can still be ReloadType::NONE in cases that
      // ShouldTreatNavigationAsReload returns false (e.g., POST, view-source).

      // If we classified this correctly, the SiteInstance should not have
      // changed.
      CHECK_EQ(entry->site_instance(), rfh->GetSiteInstance());

      // For converted reloads, we assign the entry's unique ID to be that of
      // the new one. Since this is always the result of a user action, we want
      // to dismiss infobars, etc. like a regular user-initiated navigation.
      entry->set_unique_id(pending_entry_->GetUniqueID());

      // The extra headers may have changed due to reloading with different
      // headers.
      entry->set_extra_headers(pending_entry_->extra_headers());
    }
    // Otherwise, this was intended as a new entry but the pending entry was
    // lost in the meantime and no new entry was created. We are stuck at the
    // last committed entry.

    // Even if this is a converted reload from pressing enter in the omnibox,
    // the server could redirect, requiring an update to the SSL status. If this
    // is a same document navigation, though, there's no SSLStatus in the
    // NavigationRequest so don't overwrite the existing entry's SSLStatus.
    if (!is_same_document) {
      entry->GetSSL() =
          SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));
    }

    if (params.url.SchemeIs(url::kHttpsScheme) &&
        !request->DidEncounterError()) {
      bool has_cert = !!entry->GetSSL().certificate;
      if (is_same_document) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentIntendedAsNew",
            has_cert);
      } else {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageDifferentDocumentIntendedAsNew",
            has_cert);
      }
    }
  } else if (const int nav_entry_id = request->commit_params().nav_entry_id) {
    // This is a browser-initiated navigation (back/forward/reload).
    entry = GetEntryWithUniqueID(nav_entry_id);

    if (is_same_document) {
      // There's no SSLStatus in the NavigationRequest for same document
      // navigations, so normally we leave |entry|'s SSLStatus as is. However if
      // this was a restored same document navigation entry, then it won't have
      // an SSLStatus. So we need to copy over the SSLStatus from the entry that
      // navigated it.
      NavigationEntryImpl* last_entry = GetLastCommittedEntry();
      if (entry->GetURL().DeprecatedGetOriginAsURL() ==
              last_entry->GetURL().DeprecatedGetOriginAsURL() &&
          last_entry->GetSSL().initialized && !entry->GetSSL().initialized &&
          was_restored) {
        entry->GetSSL() = last_entry->GetSSL();
      }
    } else {
      // In rapid back/forward navigations |request| sometimes won't have a cert
      // (http://crbug.com/727892). So we use the request's cert if it exists,
      // otherwise we only reuse the existing cert if the origins match.
      if (request->GetSSLInfo().has_value() &&
          request->GetSSLInfo()->is_valid()) {
        entry->GetSSL() = SSLStatus(*(request->GetSSLInfo()));
      } else if (entry->GetURL().DeprecatedGetOriginAsURL() !=
                 request->GetURL().DeprecatedGetOriginAsURL()) {
        entry->GetSSL() = SSLStatus();
      }
    }

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        !request->DidEncounterError()) {
      bool has_cert = !!entry->GetSSL().certificate;
      if (is_same_document && was_restored) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentRestoredBrowserInitiated",
            has_cert);
      } else if (is_same_document && !was_restored) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentBrowserInitiated",
            has_cert);
      } else if (!is_same_document && was_restored) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageRestoredBrowserInitiated",
            has_cert);
      } else {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus.ExistingPageBrowserInitiated",
            has_cert);
      }
    }
  } else {
    // This is renderer-initiated. The only kinds of renderer-initated
    // navigations that are EXISTING_ENTRY are same-document navigations that
    // result in replacement (e.g. history.replaceState(), location.replace(),
    // forced replacements for trivial session history contexts). For these
    // cases, we reuse the last committed entry.
    entry = GetLastCommittedEntry();

    // TODO(crbug.com/751023): Set page transition type to PAGE_TRANSITION_LINK
    // to avoid misleading interpretations (e.g. URLs paired with
    // PAGE_TRANSITION_TYPED that haven't actually been typed) as well as to fix
    // the inconsistency with what we report to observers (PAGE_TRANSITION_LINK
    // | PAGE_TRANSITION_CLIENT_REDIRECT).

    CopyReplacedNavigationEntryDataIfPreviouslyEmpty(entry, entry);

    // If this is a same document navigation, then there's no SSLStatus in the
    // NavigationRequest so don't overwrite the existing entry's SSLStatus.
    if (!is_same_document)
      entry->GetSSL() =
          SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        !request->DidEncounterError()) {
      bool has_cert = !!entry->GetSSL().certificate;
      if (is_same_document) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentRendererInitiated",
            has_cert);
      } else {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageDifferentDocumentRendererInitiated",
            has_cert);
      }
    }
  }
  DCHECK(entry);

  UpdateNavigationEntryDetails(entry, rfh, params, request,
                               NavigationEntryImpl::UpdatePolicy::kUpdate,
                               false /* is_new_entry */);

  // The redirected to page should not inherit the favicon from the previous
  // page.
  if (ui::PageTransitionIsRedirect(params.transition) && !is_same_document)
    entry->GetFavicon() = FaviconStatus();

  // We should also usually discard the pending entry if it corresponds to a
  // different navigation, since that one is now likely canceled.  In rare
  // cases, we leave the pending entry for another navigation in place when we
  // know it is still ongoing, to avoid a flicker in the omnibox (see
  // https://crbug.com/900036).
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  if (!keep_pending_entry)
    DiscardNonCommittedEntries();

  // Update the last committed index to reflect the committed entry.
  last_committed_entry_index_ = GetIndexOfEntry(entry);
}

void NavigationControllerImpl::RendererDidNavigateNewSubframe(
    RenderFrameHostImpl* rfh,
    const mojom::DidCommitProvisionalLoadParams& params,
    bool is_same_document,
    bool replace_entry,
    bool previous_document_was_activated,
    NavigationRequest* request) {
  DCHECK(ui::PageTransitionCoreTypeIs(params.transition,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME));

  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";

  // The DCHECK below documents the fact that we don't know of any situation
  // where |replace_entry| is true for subframe navigations. This simplifies
  // reasoning about the replacement struct for subframes (see
  // CopyReplacedNavigationEntryDataIfPreviouslyEmpty()).
  DCHECK(!replace_entry);

  // This FrameNavigationEntry might not end up being used in the
  // CloneAndReplace() call below, if a spot can't be found for it in the tree.
  const absl::optional<url::Origin>& initiator_origin =
      request->common_params().initiator_origin;
  auto frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
      rfh->frame_tree_node()->unique_name(), params.item_sequence_number,
      params.document_sequence_number, params.app_history_key,
      rfh->GetSiteInstance(), nullptr, params.url,
      GetCommittedOriginForFrameEntry(params, request),
      Referrer(*params.referrer), initiator_origin, request->GetRedirectChain(),
      params.page_state, params.method, params.post_id,
      nullptr /* blob_url_loader_factory */,
      request->web_bundle_navigation_info()
          ? request->web_bundle_navigation_info()->Clone()
          : nullptr,
      request->GetSubresourceWebBundleNavigationInfo(),
      ComputePolicyContainerPoliciesForFrameEntry(rfh, is_same_document,
                                                  request));

  std::unique_ptr<NavigationEntryImpl> new_entry =
      GetLastCommittedEntry()->CloneAndReplace(
          std::move(frame_entry), is_same_document, rfh->frame_tree_node(),
          frame_tree_.root());

  SetShouldSkipOnBackForwardUIIfNeeded(
      replace_entry, previous_document_was_activated,
      request->IsRendererInitiated(), request->GetPreviousPageUkmSourceId());

  // TODO(creis): Update this to add the frame_entry if we can't find the one
  // to replace, which can happen due to a unique name change. See
  // https://crbug.com/607205. For now, the call to CloneAndReplace() will
  // delete the |frame_entry| when the function exits if it doesn't get used.

  InsertOrReplaceEntry(std::move(new_entry), replace_entry, false);
}

bool NavigationControllerImpl::RendererDidNavigateAutoSubframe(
    RenderFrameHostImpl* rfh,
    const mojom::DidCommitProvisionalLoadParams& params,
    bool is_same_document,
    bool was_on_initial_empty_document,
    NavigationRequest* request) {
  DCHECK(ui::PageTransitionCoreTypeIs(params.transition,
                                      ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // We're guaranteed to have a previously committed entry, and we now need to
  // handle navigation inside of a subframe in it without creating a new entry.
  DCHECK(GetLastCommittedEntry());

  // For newly created subframes, we don't need to send a commit notification.
  // This is only necessary for history navigations in subframes.
  bool send_commit_notification = false;

  // If |nav_entry_id| is non-zero and matches an existing entry, this
  // is a history navigation.  Update the last committed index accordingly. If
  // we don't recognize the |nav_entry_id|, it might be a recently
  // pruned entry.  We'll handle it below.
  if (const int nav_entry_id = request->commit_params().nav_entry_id) {
    int entry_index = GetEntryIndexWithUniqueID(nav_entry_id);
    if (entry_index != -1 && entry_index != last_committed_entry_index_) {
      // Make sure that a subframe commit isn't changing the main frame's
      // origin. Otherwise the renderer process may be confused, leading to a
      // URL spoof. We can't check the path since that may change
      // (https://crbug.com/373041).
      // TODO(creis): For now, restrict this check to HTTP(S) origins, because
      // about:blank, file, and unique origins are more subtle to get right.
      // We should use checks similar to RenderFrameHostImpl's
      // CanCommitUrlAndOrigin on the main frame during subframe commits.
      // See https://crbug.com/1209092.
      const GURL& dest_top_url = GetEntryAtIndex(entry_index)->GetURL();
      const GURL& current_top_url = GetLastCommittedEntry()->GetURL();
      if (current_top_url.SchemeIsHTTPOrHTTPS() &&
          dest_top_url.SchemeIsHTTPOrHTTPS() &&
          current_top_url.DeprecatedGetOriginAsURL() !=
              dest_top_url.DeprecatedGetOriginAsURL()) {
        bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                        bad_message::NC_AUTO_SUBFRAME);
      }

      // We only need to discard the pending entry in this history navigation
      // case.  For newly created subframes, there was no pending entry.
      last_committed_entry_index_ = entry_index;
      DiscardNonCommittedEntries();

      // History navigations should send a commit notification.
      send_commit_notification = true;
    }
  }

  // This may be a "new auto" case where we add a new FrameNavigationEntry, or
  // it may be a "history auto" case where we update an existing one.
  NavigationEntryImpl* last_committed = GetLastCommittedEntry();

  // We may want to update |last_committed|'s FrameNavigationEntry (if one
  // exists), or we may want to clobber it and create a new one. We update in
  // cases where the corresponding FrameNavigationEntry is conceptually similar
  // to the document described by the commit params: same-document
  // navigations, history traversal to an existing entry, and reloads (including
  // a "soft reload" where we navigate to the same url without flagging it as a
  // reload). But in the case of a different document that is not logically
  // related to the committed FrameNavigationEntry's document (cross-document,
  // not same url, not a reload, not a history traversal), we replace rather
  // than update.
  //
  // In the case where we update, the FrameNavigationEntry will potentially be
  // shared across multiple NavigationEntries, and any updates will be reflected
  // in all of those NavigationEntries. In the replace case, any existing
  // sharing with other NavigationEntries will stop.
  //
  // When navigating away from the initial empty document, we also update rather
  // than replace. Either update or replace will overwrite the initial empty
  // document state for |last_committed|, but if the FrameNavigationEntry for
  // the initial empty document is shared across multiple NavigationEntries (due
  // to a navigation in another frame), we want to make sure we overwrite the
  // initial empty document state everywhere this FrameNavigationEntry is used,
  // which is accompished by updating the existing FrameNavigationEntry.
  FrameNavigationEntry* last_committed_frame_entry =
      last_committed->GetFrameEntry(rfh->frame_tree_node());
  NavigationEntryImpl::UpdatePolicy update_policy =
      NavigationEntryImpl::UpdatePolicy::kUpdate;
  if (request->common_params().navigation_type ==
          blink::mojom::NavigationType::DIFFERENT_DOCUMENT &&
      last_committed_frame_entry &&
      last_committed_frame_entry->url() != params.url &&
      !was_on_initial_empty_document) {
    update_policy = NavigationEntryImpl::UpdatePolicy::kReplace;
  }

  UpdateNavigationEntryDetails(last_committed, rfh, params, request,
                               update_policy, false /* is_new_entry */);

  return send_commit_notification;
}

int NavigationControllerImpl::GetIndexOfEntry(
    const NavigationEntryImpl* entry) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].get() == entry)
      return i;
  }
  return -1;
}

void NavigationControllerImpl::CopyStateFrom(NavigationController* temp,
                                             bool needs_reload) {
  NavigationControllerImpl* source =
      static_cast<NavigationControllerImpl*>(temp);
  // Verify that we look new.
  DCHECK_EQ(0, GetEntryCount());
  DCHECK(!GetPendingEntry());

  if (source->GetEntryCount() == 0)
    return;  // Nothing new to do.

  needs_reload_ = needs_reload;
  needs_reload_type_ = NeedsReloadType::kCopyStateFrom;
  InsertEntriesFrom(source, source->GetEntryCount());

  for (auto& it : source->session_storage_namespace_map_) {
    SessionStorageNamespaceImpl* source_namespace =
        static_cast<SessionStorageNamespaceImpl*>(it.second.get());
    session_storage_namespace_map_[it.first] = source_namespace->Clone();
    OnStoragePartitionIdAdded(it.first);
  }

  FinishRestore(source->last_committed_entry_index_, RestoreType::kRestored);
}

void NavigationControllerImpl::CopyStateFromAndPrune(NavigationController* temp,
                                                     bool replace_entry) {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());

  NavigationControllerImpl* source =
      static_cast<NavigationControllerImpl*>(temp);

  // Remove all the entries leaving the last committed entry.
  PruneAllButLastCommittedInternal();

  // We now have one entry, possibly with a new pending entry.  Ensure that
  // adding the entries from source won't put us over the limit.
  DCHECK_EQ(1, GetEntryCount());
  if (!replace_entry)
    source->PruneOldestSkippableEntryIfFull();

  // Insert the entries from source. Ignore any pending entry, since it has not
  // committed in source.
  int max_source_index = source->last_committed_entry_index_;
  if (max_source_index == -1)
    max_source_index = source->GetEntryCount();
  else
    max_source_index++;

  // Ignore the source's current entry if merging with replacement.
  // TODO(davidben): This should preserve entries forward of the current
  // too. http://crbug.com/317872
  if (replace_entry && max_source_index > 0)
    max_source_index--;

  InsertEntriesFrom(source, max_source_index);

  // Adjust indices such that the last entry and pending are at the end now.
  last_committed_entry_index_ = GetEntryCount() - 1;

  BroadcastHistoryOffsetAndLength();
}

bool NavigationControllerImpl::CanPruneAllButLastCommitted() {
  // If there is no last committed entry, we cannot prune.  Even if there is a
  // pending entry, it may not commit, leaving this WebContents blank, despite
  // possibly giving it new entries via CopyStateFromAndPrune.
  if (last_committed_entry_index_ == -1)
    return false;

  // We cannot prune if there is a pending entry at an existing entry index.
  // It may not commit, so we have to keep the last committed entry, and thus
  // there is no sensible place to keep the pending entry.  It is ok to have
  // a new pending entry, which can optionally commit as a new navigation.
  if (pending_entry_index_ != -1)
    return false;

  return true;
}

void NavigationControllerImpl::PruneAllButLastCommitted() {
  PruneAllButLastCommittedInternal();

  DCHECK_EQ(0, last_committed_entry_index_);
  DCHECK_EQ(1, GetEntryCount());

  BroadcastHistoryOffsetAndLength();
}

void NavigationControllerImpl::PruneAllButLastCommittedInternal() {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());

  // Erase all entries but the last committed entry.  There may still be a
  // new pending entry after this.
  entries_.erase(entries_.begin(),
                 entries_.begin() + last_committed_entry_index_);
  entries_.erase(entries_.begin() + 1, entries_.end());
  last_committed_entry_index_ = 0;
}

void NavigationControllerImpl::DeleteNavigationEntries(
    const DeletionPredicate& deletionPredicate) {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());
  std::vector<int> delete_indices;
  for (size_t i = 0; i < entries_.size(); i++) {
    if (i != static_cast<size_t>(last_committed_entry_index_) &&
        deletionPredicate.Run(entries_[i].get())) {
      delete_indices.push_back(i);
    }
  }
  if (delete_indices.empty())
    return;

  if (delete_indices.size() == GetEntryCount() - 1U) {
    PruneAllButLastCommitted();
  } else {
    // Do the deletion in reverse to preserve indices.
    for (auto it = delete_indices.rbegin(); it != delete_indices.rend(); ++it) {
      RemoveEntryAtIndex(*it);
    }
    BroadcastHistoryOffsetAndLength();
  }
  delegate()->NotifyNavigationEntriesDeleted();
}

bool NavigationControllerImpl::IsEntryMarkedToBeSkipped(int index) {
  auto* entry = GetEntryAtIndex(index);
  return entry && entry->should_skip_on_back_forward_ui();
}

BackForwardCacheImpl& NavigationControllerImpl::GetBackForwardCache() {
  return back_forward_cache_;
}

void NavigationControllerImpl::DiscardPendingEntry(bool was_failure) {
  // It is not safe to call DiscardPendingEntry while NavigateToEntry is in
  // progress, since this will cause a use-after-free.  (We only allow this
  // when the tab is being destroyed for shutdown, since it won't return to
  // NavigateToEntry in that case.)  http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_ || frame_tree_.IsBeingDestroyed());

  if (was_failure && pending_entry_) {
    failed_pending_entry_id_ = pending_entry_->GetUniqueID();
  } else {
    failed_pending_entry_id_ = 0;
  }

  if (pending_entry_) {
    if (pending_entry_index_ == -1)
      delete pending_entry_;
    pending_entry_index_ = -1;
    pending_entry_ = nullptr;
  }

  // Ensure any refs to the current pending entry are ignored if they get
  // deleted, by clearing the set of known refs. All future pending entries will
  // only be affected by new refs.
  pending_entry_refs_.clear();
}

void NavigationControllerImpl::SetPendingNavigationSSLError(bool error) {
  if (pending_entry_)
    pending_entry_->set_ssl_error(error);
}

#if defined(OS_ANDROID)
// static
bool NavigationControllerImpl::ValidateDataURLAsString(
    const scoped_refptr<const base::RefCountedString>& data_url_as_string) {
  if (!data_url_as_string)
    return false;

  if (data_url_as_string->size() > kMaxLengthOfDataURLString)
    return false;

  // The number of characters that is enough for validating a data: URI.
  // From the GURL's POV, the only important part here is scheme, it doesn't
  // check the actual content. Thus we can take only the prefix of the url, to
  // avoid unneeded copying of a potentially long string.
  const size_t kDataUriPrefixMaxLen = 64;
  GURL data_url(
      std::string(data_url_as_string->front_as<char>(),
                  std::min(data_url_as_string->size(), kDataUriPrefixMaxLen)));
  if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme))
    return false;

  return true;
}
#endif

void NavigationControllerImpl::NotifyUserActivation() {
  // When a user activation occurs, ensure that all adjacent entries for the
  // same document clear their skippable bit, so that the history manipulation
  // intervention does not apply to them.
  auto* last_committed_entry = GetLastCommittedEntry();
  if (!last_committed_entry)
    return;

  if (base::FeatureList::IsEnabled(
          features::kDebugHistoryInterventionNoUserActivation)) {
    return;
  }

  SetSkippableForSameDocumentEntries(GetLastCommittedEntryIndex(), false);
}

bool NavigationControllerImpl::StartHistoryNavigationInNewSubframe(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingAssociatedRemote<mojom::NavigationClient>* navigation_client) {
  NavigationEntryImpl* entry =
      GetEntryWithUniqueID(render_frame_host->nav_entry_id());
  if (!entry)
    return false;

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(render_frame_host->frame_tree_node());
  if (!frame_entry)
    return false;

  // |is_browser_initiated| is false here because a navigation in a new subframe
  // always begins with renderer action (i.e., an HTML element being inserted
  // into the DOM), so it is always renderer-initiated.
  std::unique_ptr<NavigationRequest> request = CreateNavigationRequestFromEntry(
      render_frame_host->frame_tree_node(), entry, frame_entry,
      ReloadType::NONE, false /* is_same_document_history_load */,
      true /* is_history_navigation_in_new_child */,
      false /* is_browser_initiated */);

  if (!request)
    return false;

  request->SetNavigationClient(std::move(*navigation_client));

  render_frame_host->frame_tree_node()->navigator().Navigate(std::move(request),
                                                             ReloadType::NONE);

  return true;
}

bool NavigationControllerImpl::ReloadFrame(FrameTreeNode* frame_tree_node) {
  NavigationEntryImpl* entry = GetEntryAtIndex(GetCurrentEntryIndex());
  if (!entry)
    return false;
  FrameNavigationEntry* frame_entry = entry->GetFrameEntry(frame_tree_node);
  if (!frame_entry)
    return false;
  ReloadType reload_type = ReloadType::NORMAL;
  entry->set_reload_type(reload_type);
  std::unique_ptr<NavigationRequest> request = CreateNavigationRequestFromEntry(
      frame_tree_node, entry, frame_entry, reload_type,
      false /* is_same_document_history_load */,
      false /* is_history_navigation_in_new_child */,
      true /* is_browser_initiated */);
  if (!request)
    return false;
  frame_tree_node->navigator().Navigate(std::move(request), reload_type);
  return true;
}

void NavigationControllerImpl::GoToOffsetInSandboxedFrame(
    int offset,
    int sandbox_frame_tree_node_id) {
  if (!CanGoToOffset(offset))
    return;
  GoToIndex(GetIndexForOffset(offset), sandbox_frame_tree_node_id,
            false /* is_browser_initiated */);
}

void NavigationControllerImpl::NavigateFromFrameProxy(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const blink::LocalFrameToken* initiator_frame_token,
    int initiator_process_id,
    const absl::optional<url::Origin>& initiator_origin,
    bool is_renderer_initiated,
    SiteInstance* source_site_instance,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry,
    blink::NavigationDownloadPolicy download_policy,
    const std::string& method,
    scoped_refptr<network::ResourceRequestBody> post_body,
    const std::string& extra_headers,
    network::mojom::SourceLocationPtr source_location,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    const absl::optional<blink::Impression>& impression) {
  if (is_renderer_initiated)
    DCHECK(initiator_origin.has_value());

  FrameTreeNode* node = render_frame_host->frame_tree_node();

  // Don't allow an entry replacement if there is no entry to replace.
  // http://crbug.com/457149
  if (GetEntryCount() == 0)
    should_replace_current_entry = false;

  // Create a NavigationEntry for the transfer, without making it the pending
  // entry. Subframe transfers should have a clone of the last committed entry
  // with a FrameNavigationEntry for the target frame. Main frame transfers
  // should have a new NavigationEntry.
  // TODO(creis): Make this unnecessary by creating (and validating) the params
  // directly, passing them to the destination RenderFrameHost.  See
  // https://crbug.com/536906.
  std::unique_ptr<NavigationEntryImpl> entry;
  if (!render_frame_host->is_main_frame()) {
    // Subframe case: create FrameNavigationEntry.
    if (GetLastCommittedEntry()) {
      entry = GetLastCommittedEntry()->Clone();
      entry->set_extra_headers(extra_headers);
      // TODO(arthursonzogni): What about |is_renderer_initiated|?
      // Renderer-initiated navigation that target a remote frame are currently
      // classified as browser-initiated when this one has already navigated.
      // See https://crbug.com/722251.
    } else {
      // If there's no last committed entry, create an entry for about:blank
      // with a subframe entry for our destination.
      // TODO(creis): Ensure this case can't exist in https://crbug.com/524208.
      entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
          GURL(url::kAboutBlankURL), referrer, initiator_origin,
          source_site_instance, page_transition, is_renderer_initiated,
          extra_headers, browser_context_,
          nullptr /* blob_url_loader_factory */));
    }
    // The UpdatePolicy doesn't matter here. |entry| is only used as a parameter
    // to CreateNavigationRequestFromLoadParams(), so while kReplace might
    // remove child FrameNavigationEntries (e.g., if this is a cross-process
    // same-document navigation), they will still be present in the
    // committed NavigationEntry that will be referenced to construct the new
    // FrameNavigationEntry tree when this navigation commits.
    entry->AddOrUpdateFrameEntry(
        node, NavigationEntryImpl::UpdatePolicy::kReplace, -1, -1, "", nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance), url,
        absl::nullopt /* commit_origin */, referrer, initiator_origin,
        std::vector<GURL>(), blink::PageState(), method, -1,
        blob_url_loader_factory, nullptr /* web_bundle_navigation_info */,
        nullptr /* subresource_web_bundle_navigation_info */,
        nullptr /* policy_container_policies */);
  } else {
    // Main frame case.
    entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
        url, referrer, initiator_origin, source_site_instance, page_transition,
        is_renderer_initiated, extra_headers, browser_context_,
        blob_url_loader_factory));
    entry->root_node()->frame_entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(source_site_instance));
    entry->root_node()->frame_entry->set_method(method);
  }

  bool override_user_agent = false;
  if (GetLastCommittedEntry() &&
      GetLastCommittedEntry()->GetIsOverridingUserAgent()) {
    entry->SetIsOverridingUserAgent(true);
    override_user_agent = true;
  }
  // TODO(creis): Set user gesture and intent received timestamp on Android.

  // We may not have successfully added the FrameNavigationEntry to |entry|
  // above (per https://crbug.com/608402), in which case we create it from
  // scratch.  This works because we do not depend on |frame_entry| being inside
  // |entry| during NavigateToEntry.  This will go away when we shortcut this
  // further in https://crbug.com/536906.
  scoped_refptr<FrameNavigationEntry> frame_entry(entry->GetFrameEntry(node));
  if (!frame_entry) {
    frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
        node->unique_name(), -1, -1, "", nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance), url,
        absl::nullopt /* origin */, referrer, initiator_origin,
        std::vector<GURL>(), blink::PageState(), method, -1,
        blob_url_loader_factory, nullptr /* web_bundle_navigation_info */,
        nullptr /* subresource_web_bundle_navigation_info */,
        nullptr /* policy_container_policies */);
  }

  LoadURLParams params(url);
  params.initiator_frame_token = base::OptionalFromPtr(initiator_frame_token);
  params.initiator_process_id = initiator_process_id;
  params.initiator_origin = initiator_origin;
  params.source_site_instance = source_site_instance;
  params.load_type = method == "POST" ? LOAD_TYPE_HTTP_POST : LOAD_TYPE_DEFAULT;
  params.transition_type = page_transition;
  params.frame_tree_node_id = node->frame_tree_node_id();
  params.referrer = referrer;
  /* params.redirect_chain: skip */
  params.extra_headers = extra_headers;
  params.is_renderer_initiated = is_renderer_initiated;
  params.override_user_agent = UA_OVERRIDE_INHERIT;
  /* params.base_url_for_data_url: skip */
  /* params.virtual_url_for_data_url: skip */
  /* params.data_url_as_string: skip */
  params.post_data = post_body;
  params.can_load_local_resources = false;
  /* params.should_replace_current_entry: skip */
  /* params.frame_name: skip */
  // TODO(clamy): See if user gesture should be propagated to this function.
  params.has_user_gesture = false;
  params.should_clear_history_list = false;
  params.started_from_context_menu = false;
  /* params.navigation_ui_data: skip */
  /* params.input_start: skip */
  params.was_activated = blink::mojom::WasActivatedOption::kUnknown;
  /* params.reload_type: skip */
  params.impression = impression;
  params.download_policy = std::move(download_policy);

  std::unique_ptr<NavigationRequest> request =
      CreateNavigationRequestFromLoadParams(
          node, params, override_user_agent, should_replace_current_entry,
          false /* has_user_gesture */, std::move(source_location),
          ReloadType::NONE, entry.get(), frame_entry.get());

  if (!request)
    return;

  // At this stage we are proceeding with this navigation. If this was renderer
  // initiated with user gesture, we need to make sure we clear up potential
  // remains of a cancelled browser initiated navigation to avoid URL spoofs.
  DiscardNonCommittedEntries();

  node->navigator().Navigate(std::move(request), ReloadType::NONE);
}

void NavigationControllerImpl::SetSessionStorageNamespace(
    const StoragePartitionId& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  if (!session_storage_namespace)
    return;

  // We can't overwrite an existing SessionStorage without violating spec.
  // Attempts to do so may give a tab access to another tab's session storage
  // so die hard on an error.
  bool successful_insert =
      session_storage_namespace_map_
          .insert(std::make_pair(partition_id,
                                 static_cast<SessionStorageNamespaceImpl*>(
                                     session_storage_namespace)))
          .second;
  CHECK(successful_insert) << "Cannot replace existing SessionStorageNamespace";
  OnStoragePartitionIdAdded(partition_id);
}

bool NavigationControllerImpl::IsUnmodifiedBlankTab() {
  return IsInitialNavigation() && !GetLastCommittedEntry() &&
         !frame_tree_.has_accessed_initial_main_document();
}

SessionStorageNamespace* NavigationControllerImpl::GetSessionStorageNamespace(
    const SiteInfo& site_info) {
  // TODO(acolwell): Remove partition_id logic once we have successfully
  // migrated the implementation to be a StoragePartitionConfig. At that point
  // |site_info| can be replaced with a StoragePartitionConfig.
  const StoragePartitionId partition_id =
      site_info.GetStoragePartitionId(browser_context_);
  const StoragePartitionConfig partition_config =
      site_info.storage_partition_config();

  StoragePartition* partition =
      browser_context_->GetStoragePartition(partition_config);
  DOMStorageContextWrapper* context_wrapper =
      static_cast<DOMStorageContextWrapper*>(partition->GetDOMStorageContext());

  SessionStorageNamespaceMap::const_iterator it =
      session_storage_namespace_map_.find(partition_id);
  if (it != session_storage_namespace_map_.end()) {
    // Ensure that this namespace actually belongs to this partition.
    DCHECK(static_cast<SessionStorageNamespaceImpl*>(it->second.get())
               ->IsFromContext(context_wrapper));

    // Verify that the config we generated now matches the one that
    // was generated when the namespace was added to the map.
    if (partition_config != it->first.config()) {
      LogStoragePartitionIdCrashKeys(it->first, partition_id);
    }
    CHECK_EQ(partition_config, it->first.config());

    return it->second.get();
  }

  // Create one if no one has accessed session storage for this partition yet.
  scoped_refptr<SessionStorageNamespaceImpl> session_storage_namespace =
      SessionStorageNamespaceImpl::Create(context_wrapper);
  SessionStorageNamespaceImpl* session_storage_namespace_ptr =
      session_storage_namespace.get();
  session_storage_namespace_map_[partition_id] =
      std::move(session_storage_namespace);
  OnStoragePartitionIdAdded(partition_id);

  return session_storage_namespace_ptr;
}

SessionStorageNamespace*
NavigationControllerImpl::GetDefaultSessionStorageNamespace() {
  return GetSessionStorageNamespace(SiteInfo(GetBrowserContext()));
}

const SessionStorageNamespaceMap&
NavigationControllerImpl::GetSessionStorageNamespaceMap() {
  return session_storage_namespace_map_;
}

bool NavigationControllerImpl::NeedsReload() {
  return needs_reload_;
}

void NavigationControllerImpl::SetNeedsReload() {
  SetNeedsReload(NeedsReloadType::kRequestedByClient);
}

void NavigationControllerImpl::SetNeedsReload(NeedsReloadType type) {
  needs_reload_ = true;
  needs_reload_type_ = type;

  if (last_committed_entry_index_ != -1) {
    entries_[last_committed_entry_index_]->SetTransitionType(
        ui::PAGE_TRANSITION_RELOAD);
  }
}

void NavigationControllerImpl::RemoveEntryAtIndexInternal(int index) {
  DCHECK_LT(index, GetEntryCount());
  DCHECK_NE(index, last_committed_entry_index_);
  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);
  if (last_committed_entry_index_ > index)
    last_committed_entry_index_--;
}

NavigationEntryImpl* NavigationControllerImpl::GetPendingEntry() {
  // If there is no pending_entry_, there should be no pending_entry_index_.
  DCHECK(pending_entry_ || pending_entry_index_ == -1);

  // If there is a pending_entry_index_, then pending_entry_ must be the entry
  // at that index. An exception is while a reload of a post commit error page
  // is ongoing; in that case pending entry will point to the entry replaced
  // by the error.
  DCHECK(pending_entry_index_ == -1 ||
         pending_entry_ == GetEntryAtIndex(pending_entry_index_) ||
         pending_entry_ == entry_replaced_by_post_commit_error_.get());

  return pending_entry_;
}

int NavigationControllerImpl::GetPendingEntryIndex() {
  // The pending entry index must always be less than the number of entries.
  // If there are no entries, it must be exactly -1.
  DCHECK_LT(pending_entry_index_, GetEntryCount());
  DCHECK(GetEntryCount() != 0 || pending_entry_index_ == -1);
  return pending_entry_index_;
}

void NavigationControllerImpl::InsertOrReplaceEntry(
    std::unique_ptr<NavigationEntryImpl> entry,
    bool replace,
    bool was_post_commit_error) {
  DCHECK(!ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                       ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // If the pending_entry_index_ is -1, the navigation was to a new page, and we
  // need to keep continuity with the pending entry, so copy the pending entry's
  // unique ID to the committed entry. If the pending_entry_index_ isn't -1,
  // then the renderer navigated on its own, independent of the pending entry,
  // so don't copy anything.
  if (pending_entry_ && pending_entry_index_ == -1)
    entry->set_unique_id(pending_entry_->GetUniqueID());

  DiscardNonCommittedEntries();

  // When replacing, don't prune the forward history.
  if ((replace || was_post_commit_error) && entries_.size() > 0) {
    CopyReplacedNavigationEntryDataIfPreviouslyEmpty(
        entries_[last_committed_entry_index_].get(), entry.get());
    // If the new entry is a post-commit error page, we store the current last
    // committed entry to the side so that we can put it back when navigating
    // away from the error.
    if (was_post_commit_error) {
      DCHECK(!entry_replaced_by_post_commit_error_);
      entry_replaced_by_post_commit_error_ =
          std::move(entries_[last_committed_entry_index_]);
    }
    entries_[last_committed_entry_index_] = std::move(entry);
    return;
  }

  // We shouldn't see replace == true when there's no committed entries.
  DCHECK(!replace);

  PruneForwardEntries();

  PruneOldestSkippableEntryIfFull();

  entries_.push_back(std::move(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;
}

void NavigationControllerImpl::PruneOldestSkippableEntryIfFull() {
  if (entries_.size() < max_entry_count())
    return;

  DCHECK_EQ(max_entry_count(), entries_.size());
  DCHECK_GT(last_committed_entry_index_, 0);
  CHECK_EQ(pending_entry_index_, -1);

  int index = 0;
  // Retrieve the oldest skippable entry.
  for (; index < GetEntryCount(); index++) {
    if (GetEntryAtIndex(index)->should_skip_on_back_forward_ui())
      break;
  }

  // If there is no skippable entry or if it is the last committed entry then
  // fall back to pruning the oldest entry. It is not safe to prune the last
  // committed entry.
  if (index == GetEntryCount() || index == last_committed_entry_index_)
    index = 0;

  bool should_succeed = RemoveEntryAtIndex(index);
  DCHECK_EQ(true, should_succeed);

  NotifyPrunedEntries(this, index, 1);
}

void NavigationControllerImpl::NavigateToExistingPendingEntry(
    ReloadType reload_type,
    int sandboxed_source_frame_tree_node_id,
    bool is_browser_initiated) {
  TRACE_EVENT0("navigation",
               "NavigationControllerImpl::NavigateToExistingPendingEntry");
  DCHECK(pending_entry_);
  DCHECK(IsInitialNavigation() || pending_entry_index_ != -1);
  if (pending_entry_index_ != -1) {
    // The pending entry may not be in entries_ if a post-commit error page is
    // showing.
    DCHECK(pending_entry_ == entries_[pending_entry_index_].get() ||
           pending_entry_ == entry_replaced_by_post_commit_error_.get());
  }
  DCHECK(!blink::IsRendererDebugURL(pending_entry_->GetURL()));
  bool is_forced_reload = needs_reload_;
  needs_reload_ = false;
  FrameTreeNode* root = frame_tree_.root();
  int nav_entry_id = pending_entry_->GetUniqueID();

  // If we were navigating to a slow-to-commit page, and the user performs
  // a session history navigation to the last committed page, RenderViewHost
  // will force the throbber to start, but WebKit will essentially ignore the
  // navigation, and won't send a message to stop the throbber. To prevent this
  // from happening, we drop the navigation here and stop the slow-to-commit
  // page from loading (which would normally happen during the navigation).
  if (pending_entry_index_ == last_committed_entry_index_ &&
      !pending_entry_->IsRestored() &&
      pending_entry_->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK) {
    frame_tree_.StopLoading();

    DiscardNonCommittedEntries();
    return;
  }

  // Compare FrameNavigationEntries to see which frames in the tree need to be
  // navigated.
  std::vector<std::unique_ptr<NavigationRequest>> same_document_loads;
  std::vector<std::unique_ptr<NavigationRequest>> different_document_loads;
  FindFramesToNavigate(root, reload_type, is_browser_initiated,
                       &same_document_loads, &different_document_loads);

  if (same_document_loads.empty() && different_document_loads.empty()) {
    // We were unable to match any frames to navigate.  This can happen if a
    // history navigation targets a subframe that no longer exists
    // (https://crbug.com/705550). In this case, we need to update the current
    // history entry to the pending one but keep the main document loaded.  We
    // also need to ensure that observers are informed about the updated
    // current history entry (e.g., for greying out back/forward buttons), and
    // that renderer processes update their history offsets.  The easiest way
    // to do all that is to schedule a "redundant" same-document navigation in
    // the main frame.
    //
    // Note that we don't want to remove this history entry, as it might still
    // be valid later, since a frame that it's targeting may be recreated.
    //
    // TODO(alexmos, creis): This behavior isn't ideal, as the user would
    // need to repeat history navigations until finding the one that works.
    // Consider changing this behavior to keep looking for the first valid
    // history entry that finds frames to navigate.
    std::unique_ptr<NavigationRequest> navigation_request =
        CreateNavigationRequestFromEntry(
            root, pending_entry_, pending_entry_->GetFrameEntry(root),
            ReloadType::NONE /* reload_type */,
            true /* is_same_document_history_load */,
            false /* is_history_navigation_in_new_child */,
            is_browser_initiated);
    if (!navigation_request) {
      // If this navigation cannot start, delete the pending NavigationEntry.
      DiscardPendingEntry(false);
      return;
    }
    same_document_loads.push_back(std::move(navigation_request));

    // Sanity check that we never take this branch for any kinds of reloads,
    // as those should've queued a different-document load in the main frame.
    DCHECK(!is_forced_reload);
    DCHECK_EQ(reload_type, ReloadType::NONE);
  }

  // If |sandboxed_source_frame_node_id| is valid, then track whether this
  // navigation affects any frame outside the frame's subtree.
  if (sandboxed_source_frame_tree_node_id !=
      FrameTreeNode::kFrameTreeNodeInvalidId) {
    bool navigates_inside_tree =
        DoesSandboxNavigationStayWithinSubtree(
            sandboxed_source_frame_tree_node_id, same_document_loads) &&
        DoesSandboxNavigationStayWithinSubtree(
            sandboxed_source_frame_tree_node_id, different_document_loads);
    // Count the navigations as web use counters so we can determine
    // the number of pages that trigger this.
    FrameTreeNode* sandbox_source_frame_tree_node =
        FrameTreeNode::GloballyFindByID(sandboxed_source_frame_tree_node_id);
    if (sandbox_source_frame_tree_node) {
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          sandbox_source_frame_tree_node->current_frame_host(),
          navigates_inside_tree
              ? blink::mojom::WebFeature::kSandboxBackForwardStaysWithinSubtree
              : blink::mojom::WebFeature::
                    kSandboxBackForwardAffectsFramesOutsideSubtree);
    }

    // If the navigation occurred outside the tree discard it because
    // the sandboxed frame didn't have permission to navigate outside
    // its tree. If it is possible that the navigation is both inside and
    // outside the frame tree and we discard it entirely because we don't
    // want to end up in a history state that didn't exist before.
    if (base::FeatureList::IsEnabled(
            features::kHistoryPreventSandboxedNavigation) &&
        !navigates_inside_tree) {
      DiscardPendingEntry(false);
      return;
    }
  }

  // BackForwardCache:
  // Navigate immediately if the document is in the BackForwardCache.
  if (back_forward_cache_.GetEntry(nav_entry_id)) {
    TRACE_EVENT0("navigation", "BackForwardCache_CreateNavigationRequest");
    DCHECK_EQ(reload_type, ReloadType::NONE);
    auto navigation_request = CreateNavigationRequestFromEntry(
        root, pending_entry_, pending_entry_->GetFrameEntry(root),
        ReloadType::NONE, false /* is_same_document_history_load */,
        false /* is_history_navigation_in_new_child */, is_browser_initiated);
    root->navigator().Navigate(std::move(navigation_request), ReloadType::NONE);

    return;
  }

  // History navigation might try to reuse a specific BrowsingInstance, already
  // used by a page in the cache. To avoid having two different main frames that
  // live in the same BrowsingInstance, evict the all pages with this
  // BrowsingInstance from the cache.
  //
  // For example, take the following scenario:
  //
  // A1 = Some page on a.com
  // A2 = Some other page on a.com
  // B3 = An uncacheable page on b.com
  //
  // Then the following navigations occur:
  // A1->A2->B3->A1
  // On the navigation from B3 to A1, A2 will remain in the cache (B3 doesn't
  // take its place) and A1 will be created in the same BrowsingInstance (and
  // SiteInstance), as A2.
  //
  // If we didn't do anything, both A1 and A2 would remain alive in the same
  // BrowsingInstance/SiteInstance, which is unsupported by
  // RenderFrameHostManager::CommitPending(). To avoid this conundrum, we evict
  // A2 from the cache.
  if (pending_entry_->site_instance()) {
    back_forward_cache_.EvictFramesInRelatedSiteInstances(
        pending_entry_->site_instance());
  }

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;

  // It is not possible to delete the pending NavigationEntry while navigating
  // to it. Grab a reference to delay potential deletion until the end of this
  // function.
  std::unique_ptr<PendingEntryRef> pending_entry_ref = ReferencePendingEntry();

  // Send all the same document frame loads before the different document loads.
  for (auto& item : same_document_loads) {
    FrameTreeNode* frame = item->frame_tree_node();
    frame->navigator().Navigate(std::move(item), reload_type);
  }
  for (auto& item : different_document_loads) {
    FrameTreeNode* frame = item->frame_tree_node();
    frame->navigator().Navigate(std::move(item), reload_type);
  }

  in_navigate_to_pending_entry_ = false;
}

NavigationControllerImpl::HistoryNavigationAction
NavigationControllerImpl::DetermineActionForHistoryNavigation(
    FrameTreeNode* frame,
    ReloadType reload_type) {
  RenderFrameHostImpl* render_frame_host = frame->current_frame_host();
  // Only active and prerendered documents are allowed to navigate in their
  // frame.
  if (render_frame_host->lifecycle_state() !=
      RenderFrameHostImpl::LifecycleStateImpl::kPrerendering) {
    // - If the document is in pending deletion, the browser already committed
    // to destroying this RenderFrameHost. See https://crbug.com/930278.
    // - If the document is in back-forward cache, it's not allowed to navigate
    // as it should remain frozen. Ignore the request and evict the document
    // from back-forward cache.
    //
    // If the document is inactive, there's no need to recurse into subframes,
    // which should all be inactive as well.
    if (frame->current_frame_host()->IsInactiveAndDisallowActivation(
            DisallowActivationReasonId::kDetermineActionForHistoryNavigation)) {
      return HistoryNavigationAction::kStopLooking;
    }
  }

  // If there's no last committed entry, there is no previous history entry to
  // compare against, so fall back to a different-document load.  Note that we
  // should only reach this case for the root frame and not descend further
  // into subframes.
  if (!GetLastCommittedEntry()) {
    DCHECK(frame->IsMainFrame());
    return HistoryNavigationAction::kDifferentDocument;
  }

  // Reloads should result in a different-document load.  Note that reloads may
  // also happen via the |needs_reload_| mechanism where the reload_type is
  // NONE, so detect this by comparing whether we're going to the same
  // entry that we're currently on.  Similarly to above, only main frames
  // should reach this.  Note that subframes support reloads, but that's done
  // via a different path that doesn't involve FindFramesToNavigate (see
  // RenderFrameHost::Reload()).
  if (reload_type != ReloadType::NONE ||
      pending_entry_index_ == last_committed_entry_index_) {
    DCHECK(frame->IsMainFrame());
    return HistoryNavigationAction::kDifferentDocument;
  }

  // If there is no new FrameNavigationEntry for the frame, ignore the
  // load.  For example, this may happen when going back to an entry before a
  // frame was created.  Suppose we commit a same-document navigation that also
  // results in adding a new subframe somewhere in the tree.  If we go back,
  // the new subframe will be missing a FrameNavigationEntry in the previous
  // NavigationEntry, but we shouldn't delete or change what's loaded in
  // it.
  //
  // Note that in this case, there is no need to keep looking for navigations
  // in subframes, which would be missing FrameNavigationEntries as well.
  //
  // It's important to check this before checking |old_item| below, since both
  // might be null, and in that case we still shouldn't change what's loaded in
  // this frame.  Note that scheduling any loads assumes that |new_item| is
  // non-null.  See https://crbug.com/1088354.
  FrameNavigationEntry* new_item = pending_entry_->GetFrameEntry(frame);
  if (!new_item)
    return HistoryNavigationAction::kStopLooking;

  // If there is no old FrameNavigationEntry, schedule a different-document
  // load.
  //
  // TODO(creis): Store the last committed FrameNavigationEntry to use here,
  // rather than assuming the NavigationEntry has up to date info on subframes.
  FrameNavigationEntry* old_item =
      GetLastCommittedEntry()->GetFrameEntry(frame);
  if (!old_item)
    return HistoryNavigationAction::kDifferentDocument;

  // If the new item is not in the same SiteInstance, schedule a
  // different-document load.  Newly restored items may not have a SiteInstance
  // yet, in which case it will be assigned on first commit.
  if (new_item->site_instance() &&
      new_item->site_instance() != old_item->site_instance())
    return HistoryNavigationAction::kDifferentDocument;

  // Schedule a different-document load if the current RenderFrameHost is not
  // live. This case can happen for Ctrl+Back or after a renderer crash. Note
  // that we do this even if the history navigation would not be modifying this
  // frame were it live.
  if (!frame->current_frame_host()->IsRenderFrameLive())
    return HistoryNavigationAction::kDifferentDocument;

  if (new_item->item_sequence_number() != old_item->item_sequence_number()) {
    // Starting a navigation after a crash early-promotes the speculative
    // RenderFrameHost. Then we have a RenderFrameHost with no document in it
    // committed yet, so we can not possibly perform a same-document history
    // navigation. The frame would need to be reloaded with a cross-document
    // navigation.
    if (!frame->current_frame_host()->has_committed_any_navigation())
      return HistoryNavigationAction::kDifferentDocument;

    // Same document loads happen if the previous item has the same document
    // sequence number but different item sequence number.
    if (new_item->document_sequence_number() ==
        old_item->document_sequence_number()) {
      return HistoryNavigationAction::kSameDocument;
    }

    // Otherwise, if both item and document sequence numbers differ, this
    // should be a different document load.
    return HistoryNavigationAction::kDifferentDocument;
  }

  // If the item sequence numbers match, there is no need to navigate this
  // frame.  Keep looking for navigations in this frame's children.
  DCHECK_EQ(new_item->document_sequence_number(),
            old_item->document_sequence_number());
  return HistoryNavigationAction::kKeepLooking;
}

void NavigationControllerImpl::FindFramesToNavigate(
    FrameTreeNode* frame,
    ReloadType reload_type,
    bool is_browser_initiated,
    std::vector<std::unique_ptr<NavigationRequest>>* same_document_loads,
    std::vector<std::unique_ptr<NavigationRequest>>* different_document_loads) {
  DCHECK(pending_entry_);
  FrameNavigationEntry* new_item = pending_entry_->GetFrameEntry(frame);

  auto action = DetermineActionForHistoryNavigation(frame, reload_type);

  if (action == HistoryNavigationAction::kSameDocument) {
    std::unique_ptr<NavigationRequest> navigation_request =
        CreateNavigationRequestFromEntry(
            frame, pending_entry_, new_item, reload_type,
            true /* is_same_document_history_load */,
            false /* is_history_navigation_in_new_child */,
            is_browser_initiated);
    if (navigation_request) {
      // Only add the request if was properly created. It's possible for the
      // creation to fail in certain cases, e.g. when the URL is invalid.
      same_document_loads->push_back(std::move(navigation_request));
    }
  } else if (action == HistoryNavigationAction::kDifferentDocument) {
    std::unique_ptr<NavigationRequest> navigation_request =
        CreateNavigationRequestFromEntry(
            frame, pending_entry_, new_item, reload_type,
            false /* is_same_document_history_load */,
            false /* is_history_navigation_in_new_child */,
            is_browser_initiated);
    if (navigation_request) {
      // Only add the request if was properly created. It's possible for the
      // creation to fail in certain cases, e.g. when the URL is invalid.
      different_document_loads->push_back(std::move(navigation_request));
    }
    // For a different document, the subframes will be destroyed, so there's
    // no need to consider them.
    return;
  } else if (action == HistoryNavigationAction::kStopLooking) {
    return;
  }

  for (size_t i = 0; i < frame->child_count(); i++) {
    FindFramesToNavigate(frame->child_at(i), reload_type, is_browser_initiated,
                         same_document_loads, different_document_loads);
  }
}

base::WeakPtr<NavigationHandle> NavigationControllerImpl::NavigateWithoutEntry(
    const LoadURLParams& params) {
  // Find the appropriate FrameTreeNode.
  FrameTreeNode* node = nullptr;
  if (params.frame_tree_node_id != RenderFrameHost::kNoFrameTreeNodeId ||
      !params.frame_name.empty()) {
    node = params.frame_tree_node_id != RenderFrameHost::kNoFrameTreeNodeId
               ? frame_tree_.FindByID(params.frame_tree_node_id)
               : frame_tree_.FindByName(params.frame_name);
    DCHECK(!node || node->frame_tree() == &frame_tree_);
  }

  // If no FrameTreeNode was specified, navigate the main frame.
  if (!node)
    node = frame_tree_.root();

  // Compute overrides to the LoadURLParams for |override_user_agent|,
  // |should_replace_current_entry| and |has_user_gesture| that will be used
  // both in the creation of the NavigationEntry and the NavigationRequest.
  // Ideally, the LoadURLParams themselves would be updated, but since they are
  // passed as a const reference, this is not possible.
  // TODO(clamy): When we only create a NavigationRequest, move this to
  // CreateNavigationRequestFromLoadURLParams.
  bool override_user_agent = ShouldOverrideUserAgent(params.override_user_agent,
                                                     GetLastCommittedEntry());

  // Don't allow an entry replacement if there is no entry to replace.
  // http://crbug.com/457149
  //
  // If there is an entry, an entry replacement must happen if the current
  // browsing context should maintain a trivial session history.
  bool should_replace_current_entry = (params.should_replace_current_entry ||
                                       ShouldMaintainTrivialSessionHistory()) &&
                                      entries_.size();

  // Javascript URLs should not create NavigationEntries. All other navigations
  // do, including navigations to chrome renderer debug URLs.
  if (!params.url.SchemeIs(url::kJavaScriptScheme)) {
    std::unique_ptr<NavigationEntryImpl> entry =
        CreateNavigationEntryFromLoadParams(node, params, override_user_agent,
                                            should_replace_current_entry,
                                            params.has_user_gesture);
    DiscardPendingEntry(false);
    SetPendingEntry(std::move(entry));
  }

  // Renderer-debug URLs are sent to the renderer process immediately for
  // processing and don't need to create a NavigationRequest.
  // Note: this includes navigations to JavaScript URLs, which are considered
  // renderer-debug URLs.
  // Note: we intentionally leave the pending entry in place for renderer debug
  // URLs, unlike the cases below where we clear it if the navigation doesn't
  // proceed.
  if (blink::IsRendererDebugURL(params.url)) {
    // Renderer-debug URLs won't go through NavigationThrottlers so we have to
    // check them explicitly. See bug 913334.
    if (GetContentClient()->browser()->ShouldBlockRendererDebugURL(
            params.url, browser_context_)) {
      DiscardPendingEntry(false);
      return nullptr;
    }

    HandleRendererDebugURL(node, params.url);
    return nullptr;
  }

  DCHECK(pending_entry_);

  // Convert navigations to the current URL to a reload.
  // TODO(clamy): We should be using FrameTreeNode::IsMainFrame here instead of
  // relying on the frame tree node id from LoadURLParams. Unfortunately,
  // DevTools sometimes issues navigations to main frames that they do not
  // expect to see treated as reload, and it only works because they pass a
  // FrameTreeNode id in their LoadURLParams. Change this once they no longer do
  // that. See https://crbug.com/850926.
  ReloadType reload_type = params.reload_type;
  if (reload_type == ReloadType::NONE &&
      ShouldTreatNavigationAsReload(
          node, params.url, pending_entry_->GetVirtualURL(),
          params.base_url_for_data_url, params.transition_type,
          params.load_type ==
              NavigationController::LOAD_TYPE_HTTP_POST /* is_post */,
          should_replace_current_entry, GetLastCommittedEntry())) {
    reload_type = ReloadType::NORMAL;
    pending_entry_->set_reload_type(reload_type);

    // If this is a reload of an existing FrameNavigationEntry and we had a
    // policy container for it, then we should copy it into the pending entry,
    // so that it is copied to the navigation request in
    // CreateNavigationRequestFromLoadParams later.
    if (GetLastCommittedEntry()) {
      FrameNavigationEntry* previous_frame_entry =
          GetLastCommittedEntry()->GetFrameEntry(node);
      if (previous_frame_entry &&
          previous_frame_entry->policy_container_policies()) {
        pending_entry_->GetFrameEntry(node)->set_policy_container_policies(
            previous_frame_entry->policy_container_policies()->Clone());
      }
    }
  }

  // If this navigation is an "Enter-in-omnibox" with the initial about:blank
  // document (so no last commit), then we should copy the document polices from
  // RenderFrameHost's PolicyContainerHost. The NavigationRequest will create a
  // new PolicyContainerHost with the document policies from the
  // |pending_entry_|, and that PolicyContainerHost will be put in the final
  // RenderFrameHost for the navigation. This way, we ensure that we keep
  // enforcing the right policies on the initial empty document after the
  // reload.
  if (!GetLastCommittedEntry() && params.url.IsAboutBlank()) {
    if (node->current_frame_host() &&
        node->current_frame_host()->policy_container_host()) {
      pending_entry_->GetFrameEntry(node)->set_policy_container_policies(
          node->current_frame_host()
              ->policy_container_host()
              ->policies()
              .Clone());
    }
  }

  // navigation_ui_data should only be present for main frame navigations.
  DCHECK(node->IsMainFrame() || !params.navigation_ui_data);

  std::unique_ptr<NavigationRequest> request =
      CreateNavigationRequestFromLoadParams(
          node, params, override_user_agent, should_replace_current_entry,
          params.has_user_gesture, network::mojom::SourceLocation::New(),
          reload_type, pending_entry_, pending_entry_->GetFrameEntry(node));

  // If the navigation couldn't start, return immediately and discard the
  // pending NavigationEntry.
  if (!request) {
    DiscardPendingEntry(false);
    return nullptr;
  }

#if DCHECK_IS_ON()
  // Safety check that NavigationRequest and NavigationEntry match.
  ValidateRequestMatchesEntry(request.get(), pending_entry_);
#endif

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;

  // It is not possible to delete the pending NavigationEntry while navigating
  // to it. Grab a reference to delay potential deletion until the end of this
  // function.
  std::unique_ptr<PendingEntryRef> pending_entry_ref = ReferencePendingEntry();

  base::WeakPtr<NavigationHandle> created_navigation_handle(
      request->GetWeakPtr());
  node->navigator().Navigate(std::move(request), reload_type);

  in_navigate_to_pending_entry_ = false;
  return created_navigation_handle;
}

void NavigationControllerImpl::HandleRendererDebugURL(
    FrameTreeNode* frame_tree_node,
    const GURL& url) {
  if (!frame_tree_node->current_frame_host()->IsRenderFrameLive()) {
    // Any renderer-side debug URLs or javascript: URLs should be ignored if
    // the renderer process is not live, unless it is the initial navigation
    // of the tab.
    if (!IsInitialNavigation()) {
      DiscardNonCommittedEntries();
      return;
    }
    // The current frame is always a main frame. If IsInitialNavigation() is
    // true then there have been no navigations and any frames of this tab must
    // be in the same renderer process. If that has crashed then the only frame
    // that can be revived is the main frame.
    frame_tree_node->render_manager()
        ->InitializeMainRenderFrameForImmediateUse();
  }
  frame_tree_node->current_frame_host()->HandleRendererDebugURL(url);
}

std::unique_ptr<NavigationEntryImpl>
NavigationControllerImpl::CreateNavigationEntryFromLoadParams(
    FrameTreeNode* node,
    const LoadURLParams& params,
    bool override_user_agent,
    bool should_replace_current_entry,
    bool has_user_gesture) {
  // Browser initiated navigations might not have a blob_url_loader_factory set
  // in params even if the navigation is to a blob URL. If that happens, lookup
  // the correct url loader factory to use here.
  auto blob_url_loader_factory = params.blob_url_loader_factory;
  if (!blob_url_loader_factory && params.url.SchemeIsBlob()) {
    // Resolve the blob URL in the storage partition associated with the target
    // frame. This is the storage partition the URL will be loaded in, and only
    // URLs that can be resolved by it should be able to access its data.
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        node->current_frame_host()->GetStoragePartition(), params.url);
  }

  std::unique_ptr<NavigationEntryImpl> entry;
  // extra_headers in params are \n separated; navigation entries want \r\n.
  std::string extra_headers_crlf;
  base::ReplaceChars(params.extra_headers, "\n", "\r\n", &extra_headers_crlf);

  // For subframes, create a pending entry with a corresponding frame entry.
  if (!node->IsMainFrame()) {
    if (GetLastCommittedEntry()) {
      entry = GetLastCommittedEntry()->Clone();
    } else {
      // If there's no last committed entry, create an entry for about:blank
      // with a subframe entry for our destination.
      // TODO(creis): Ensure this case can't exist in https://crbug.com/524208.
      entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
          GURL(url::kAboutBlankURL), params.referrer, params.initiator_origin,
          params.source_site_instance.get(), params.transition_type,
          params.is_renderer_initiated, extra_headers_crlf, browser_context_,
          blob_url_loader_factory));
    }

    entry->AddOrUpdateFrameEntry(
        node, NavigationEntryImpl::UpdatePolicy::kReplace, -1, -1, "", nullptr,
        static_cast<SiteInstanceImpl*>(params.source_site_instance.get()),
        params.url, absl::nullopt, params.referrer, params.initiator_origin,
        params.redirect_chain, blink::PageState(), "GET", -1,
        blob_url_loader_factory, nullptr /* web_bundle_navigation_info */,
        nullptr /* subresource_web_bundle_navigation_info */,
        // If in NavigateWithoutEntry we later determine that this navigation is
        // a conversion of a new navigation into a reload, we will set the right
        // document policies there.
        nullptr /* policy_container_policies */);
  } else {
    // Otherwise, create a pending entry for the main frame.
    entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
        params.url, params.referrer, params.initiator_origin,
        params.source_site_instance.get(), params.transition_type,
        params.is_renderer_initiated, extra_headers_crlf, browser_context_,
        blob_url_loader_factory));
    entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(params.source_site_instance.get()));
    entry->SetRedirectChain(params.redirect_chain);
  }

  // Set the FTN ID (only used in non-site-per-process, for tests).
  entry->set_frame_tree_node_id(node->frame_tree_node_id());
  entry->set_should_clear_history_list(params.should_clear_history_list);
  entry->SetIsOverridingUserAgent(override_user_agent);
  entry->set_has_user_gesture(has_user_gesture);
  entry->set_reload_type(params.reload_type);

  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_HTTP_POST:
      entry->SetHasPostData(true);
      entry->SetPostData(params.post_data);
      break;
    case LOAD_TYPE_DATA:
      entry->SetBaseURLForDataURL(params.base_url_for_data_url);
      entry->SetVirtualURL(params.virtual_url_for_data_url);
#if defined(OS_ANDROID)
      entry->SetDataURLAsString(params.data_url_as_string);
#endif
      entry->SetCanLoadLocalResources(params.can_load_local_resources);
      break;
  }

  // TODO(clamy): NavigationEntry is meant for information that will be kept
  // after the navigation ended and therefore is not appropriate for
  // started_from_context_menu. Move started_from_context_menu to
  // NavigationUIData.
  entry->set_started_from_context_menu(params.started_from_context_menu);

  return entry;
}

std::unique_ptr<NavigationRequest>
NavigationControllerImpl::CreateNavigationRequestFromLoadParams(
    FrameTreeNode* node,
    const LoadURLParams& params,
    bool override_user_agent,
    bool should_replace_current_entry,
    bool has_user_gesture,
    network::mojom::SourceLocationPtr source_location,
    ReloadType reload_type,
    NavigationEntryImpl* entry,
    FrameNavigationEntry* frame_entry) {
  DCHECK_EQ(-1, GetIndexOfEntry(entry));
  DCHECK(frame_entry);
  // All renderer-initiated navigations must have an initiator_origin.
  DCHECK(!params.is_renderer_initiated || params.initiator_origin.has_value());

  GURL url_to_load;
  GURL virtual_url;
  absl::optional<url::Origin> origin_to_commit =
      frame_entry ? frame_entry->committed_origin() : absl::nullopt;

  // For main frames, rewrite the URL if necessary and compute the virtual URL
  // that should be shown in the address bar.
  if (node->IsMainFrame()) {
    bool ignored_reverse_on_redirect = false;
    RewriteUrlForNavigation(params.url, browser_context_, &url_to_load,
                            &virtual_url, &ignored_reverse_on_redirect);

    // For DATA loads, override the virtual URL.
    if (params.load_type == LOAD_TYPE_DATA)
      virtual_url = params.virtual_url_for_data_url;

    if (virtual_url.is_empty())
      virtual_url = url_to_load;

    CHECK(virtual_url == entry->GetVirtualURL());

    // This is a LOG and not a CHECK/DCHECK as URL rewrite has non-deterministic
    // behavior: it is possible for two calls to RewriteUrlForNavigation to
    // return different results, leading to a different URL in the
    // NavigationRequest and FrameEntry. This will be fixed once we remove the
    // pending NavigationEntry, as we'll only make one call to
    // RewriteUrlForNavigation.
    VLOG_IF(1, (url_to_load != frame_entry->url()))
        << "NavigationRequest and FrameEntry have different URLs: "
        << url_to_load << " vs " << frame_entry->url();

    // TODO(clamy): In order to remove the pending NavigationEntry,
    // |virtual_url| and |ignored_reverse_on_redirect| should be stored in the
    // NavigationRequest.
  } else {
    url_to_load = params.url;
    virtual_url = params.url;
    CHECK(!frame_entry || url_to_load == frame_entry->url());
  }

  if (node->render_manager()->is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this node is now preparing for
    // attaching an inner delegate.
    return nullptr;
  }

  if (!IsValidURLForNavigation(node->IsMainFrame(), virtual_url, url_to_load))
    return nullptr;

  if (!DoesURLMatchOriginForNavigation(
          url_to_load, origin_to_commit,
          frame_entry->subresource_web_bundle_navigation_info())) {
    DCHECK(false) << " url:" << url_to_load
                  << " origin:" << origin_to_commit.value();
    return nullptr;
  }

  // Determine if Previews should be used for the navigation.
  blink::PreviewsState previews_state =
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED;
  if (!node->IsMainFrame()) {
    // For subframes, use the state of the top-level frame.
    previews_state = node->frame_tree()
                         ->root()
                         ->current_frame_host()
                         ->last_navigation_previews_state();
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for browser navigations in new tabs (intents, tabs opened through
  // "Open link in new tab"). If the navigation must wait on the current
  // RenderFrameHost to execute its BeforeUnload event, the navigation start
  // will be updated when the BeforeUnload ack is received.
  base::TimeTicks navigation_start = base::TimeTicks::Now();

  // Look for a pending commit that is to another document in this
  // FrameTreeNode. If one exists, then the last committed URL will not be the
  // current URL by the time this navigation commits.
  bool has_pending_cross_document_commit =
      node->render_manager()->HasPendingCommitForCrossDocumentNavigation();
  bool is_currently_error_page = node->current_frame_host()->IsErrorDocument();

  blink::mojom::NavigationType navigation_type = GetNavigationType(
      /*old_url=*/node->current_url(),
      /*new_url=*/url_to_load, reload_type, entry, *frame_entry,
      has_pending_cross_document_commit, is_currently_error_page,
      /*is_same_document_history_load=*/false);

  // Create the NavigationParams based on |params|.

  bool is_view_source_mode = entry->IsViewSourceMode();
  DCHECK_EQ(is_view_source_mode, virtual_url.SchemeIs(kViewSourceScheme));

  blink::NavigationDownloadPolicy download_policy = params.download_policy;
  // Update |download_policy| if the virtual URL is view-source.
  if (is_view_source_mode)
    download_policy.SetDisallowed(blink::NavigationDownloadType::kViewSource);

  blink::mojom::CommonNavigationParamsPtr common_params =
      blink::mojom::CommonNavigationParams::New(
          url_to_load, params.initiator_origin,
          blink::mojom::Referrer::New(params.referrer.url,
                                      params.referrer.policy),
          params.transition_type, navigation_type, download_policy,
          should_replace_current_entry, params.base_url_for_data_url,
          previews_state, navigation_start,
          params.load_type == LOAD_TYPE_HTTP_POST ? "POST" : "GET",
          params.post_data, std::move(source_location),
          params.started_from_context_menu, has_user_gesture,
          false /* has_text_fragment_token */,
          network::mojom::CSPDisposition::CHECK, std::vector<int>(),
          params.href_translate,
          false /* is_history_navigation_in_new_child_frame */,
          params.input_start, network::mojom::RequestDestination::kEmpty);

  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::mojom::CommitNavigationParams::New(
          frame_entry->committed_origin(),
          // The correct storage key will be computed before committing the
          // navigation.
          blink::StorageKey(), network::mojom::WebSandboxFlags(),
          override_user_agent, params.redirect_chain,
          std::vector<network::mojom::URLResponseHeadPtr>(),
          std::vector<net::RedirectInfo>(),
          std::string() /* post_content_type */, common_params->url,
          common_params->method, params.can_load_local_resources,
          frame_entry->page_state().ToEncodedData(), entry->GetUniqueID(),
          entry->GetSubframeUniqueNames(node), true /* intended_as_new_entry */,
          -1 /* pending_history_list_offset */,
          params.should_clear_history_list ? -1 : GetLastCommittedEntryIndex(),
          params.should_clear_history_list ? 0 : GetEntryCount(),
          false /* was_discarded */, is_view_source_mode,
          params.should_clear_history_list,
          blink::mojom::NavigationTiming::New(),
          blink::mojom::WasActivatedOption::kUnknown,
          base::UnguessableToken::Create() /* navigation_token */,
          std::vector<blink::mojom::PrefetchedSignedExchangeInfoPtr>(),
#if defined(OS_ANDROID)
          std::string(), /* data_url_as_string */
#endif
          !params.is_renderer_initiated, /* is_browser_initiated */
          GURL() /* web_bundle_physical_url */,
          GURL() /* base_url_override_for_web_bundle */,
          ukm::kInvalidSourceId /* document_ukm_source_id */,
          node->pending_frame_policy(),
          std::vector<std::string>() /* force_enabled_origin_trials */,
          false /* origin_agent_cluster */,
          std::vector<
              network::mojom::WebClientHintsType>() /* enabled_client_hints */,
          false /* is_cross_browsing_instance */, nullptr /* old_page_info */,
          -1 /* http_response_code */,
          std::vector<blink::mojom::
                          AppHistoryEntryPtr>() /* app_history_back_entries */,
          std::vector<
              blink::mojom::
                  AppHistoryEntryPtr>() /* app_history_forward_entries */,
          std::vector<GURL>() /* early_hints_preloaded_resources */,
          // This timestamp will be populated when the commit IPC is sent.
          base::TimeTicks() /* commit_sent */);
#if defined(OS_ANDROID)
  if (ValidateDataURLAsString(params.data_url_as_string)) {
    commit_params->data_url_as_string = params.data_url_as_string->data();
  }
#endif

  commit_params->was_activated = params.was_activated;

  // A form submission may happen here if the navigation is a renderer-initiated
  // form submission that took the OpenURL path.
  scoped_refptr<network::ResourceRequestBody> request_body = params.post_data;

  // extra_headers in params are \n separated; NavigationRequests want \r\n.
  std::string extra_headers_crlf;
  base::ReplaceChars(params.extra_headers, "\n", "\r\n", &extra_headers_crlf);

  auto navigation_request = NavigationRequest::CreateBrowserInitiated(
      node, std::move(common_params), std::move(commit_params),
      !params.is_renderer_initiated, params.was_opener_suppressed,
      params.initiator_frame_token.has_value()
          ? &(params.initiator_frame_token.value())
          : nullptr,
      params.initiator_process_id, extra_headers_crlf, frame_entry, entry,
      request_body,
      params.navigation_ui_data ? params.navigation_ui_data->Clone() : nullptr,
      params.impression, params.is_pdf);
  navigation_request->set_from_download_cross_origin_redirect(
      params.from_download_cross_origin_redirect);
  return navigation_request;
}

std::unique_ptr<NavigationRequest>
NavigationControllerImpl::CreateNavigationRequestFromEntry(
    FrameTreeNode* frame_tree_node,
    NavigationEntryImpl* entry,
    FrameNavigationEntry* frame_entry,
    ReloadType reload_type,
    bool is_same_document_history_load,
    bool is_history_navigation_in_new_child_frame,
    bool is_browser_initiated) {
  DCHECK(frame_entry);
  GURL dest_url = frame_entry->url();
  absl::optional<url::Origin> origin_to_commit =
      frame_entry->committed_origin();

  Referrer dest_referrer = frame_entry->referrer();
  if (reload_type == ReloadType::ORIGINAL_REQUEST_URL &&
      entry->GetOriginalRequestURL().is_valid() && !entry->GetHasPostData()) {
    // We may have been redirected when navigating to the current URL.
    // Use the URL the user originally intended to visit as signaled by the
    // ReloadType, if it's valid and if a POST wasn't involved; the latter
    // case avoids issues with sending data to the wrong page.
    dest_url = entry->GetOriginalRequestURL();
    dest_referrer = Referrer();
    origin_to_commit.reset();
  }

  if (frame_tree_node->render_manager()->is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this node is now preparing for
    // attaching an inner delegate.
    return nullptr;
  }

  if (!IsValidURLForNavigation(frame_tree_node->IsMainFrame(),
                               entry->GetVirtualURL(), dest_url)) {
    return nullptr;
  }

  if (!DoesURLMatchOriginForNavigation(
          dest_url, origin_to_commit,
          frame_entry->subresource_web_bundle_navigation_info())) {
    DCHECK(false) << " url:" << dest_url
                  << " origin:" << origin_to_commit.value();
    return nullptr;
  }

  // Determine if Previews should be used for the navigation.
  blink::PreviewsState previews_state =
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED;
  if (!frame_tree_node->IsMainFrame()) {
    // For subframes, use the state of the top-level frame.
    previews_state = frame_tree_node->frame_tree()
                         ->root()
                         ->current_frame_host()
                         ->last_navigation_previews_state();
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for browser navigations in new tabs (intents, tabs opened through
  // "Open link in new tab"). If the navigation must wait on the current
  // RenderFrameHost to execute its BeforeUnload event, the navigation start
  // will be updated when the BeforeUnload ack is received.
  base::TimeTicks navigation_start = base::TimeTicks::Now();

  // Look for a pending commit that is to another document in this
  // FrameTreeNode. If one exists, then the last committed URL will not be the
  // current URL by the time this navigation commits.
  bool has_pending_cross_document_commit =
      frame_tree_node->render_manager()
          ->HasPendingCommitForCrossDocumentNavigation();
  bool is_currently_error_page =
      frame_tree_node->current_frame_host()->IsErrorDocument();

  blink::mojom::NavigationType navigation_type = GetNavigationType(
      /*old_url=*/frame_tree_node->current_url(),
      /*new_url=*/dest_url, reload_type, entry, *frame_entry,
      has_pending_cross_document_commit, is_currently_error_page,
      is_same_document_history_load);

  // A form submission may happen here if the navigation is a
  // back/forward/reload navigation that does a form resubmission.
  scoped_refptr<network::ResourceRequestBody> request_body;
  std::string post_content_type;
  if (frame_entry->method() == "POST") {
    request_body = frame_entry->GetPostData(&post_content_type);
    // Might have a LF at end.
    post_content_type = std::string(
        base::TrimWhitespaceASCII(post_content_type, base::TRIM_ALL));
  }

  // Create the NavigationParams based on |entry| and |frame_entry|.
  blink::mojom::CommonNavigationParamsPtr common_params =
      entry->ConstructCommonNavigationParams(
          *frame_entry, request_body, dest_url,
          blink::mojom::Referrer::New(dest_referrer.url, dest_referrer.policy),
          navigation_type, previews_state, navigation_start,
          base::TimeTicks() /* input_start */);
  common_params->is_history_navigation_in_new_child_frame =
      is_history_navigation_in_new_child_frame;

  // TODO(clamy): |intended_as_new_entry| below should always be false once
  // Reload no longer leads to this being called for a pending NavigationEntry
  // of index -1.
  blink::mojom::CommitNavigationParamsPtr commit_params =
      entry->ConstructCommitNavigationParams(
          *frame_entry, common_params->url, origin_to_commit,
          common_params->method, entry->GetSubframeUniqueNames(frame_tree_node),
          GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
          GetIndexOfEntry(entry), GetLastCommittedEntryIndex(), GetEntryCount(),
          frame_tree_node->pending_frame_policy());
  commit_params->post_content_type = post_content_type;

  return NavigationRequest::CreateBrowserInitiated(
      frame_tree_node, std::move(common_params), std::move(commit_params),
      is_browser_initiated, false /* was_opener_suppressed */,
      nullptr /* initiator_frame_token */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      entry->extra_headers(), frame_entry, entry, request_body,
      nullptr /* navigation_ui_data */, absl::nullopt /* impression */,
      false /* is_pdf */);
}

void NavigationControllerImpl::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
  details->entry = GetLastCommittedEntry();

  // We need to notify the ssl_manager_ before the WebContents so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(*details);

  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
  delegate_->NotifyNavigationEntryCommitted(*details);

  // TODO(avi): Remove. http://crbug.com/170921
  NotificationDetails notification_details =
      Details<LoadCommittedDetails>(details);
  NotificationService::current()->Notify(NOTIFICATION_NAV_ENTRY_COMMITTED,
                                         Source<NavigationController>(this),
                                         notification_details);
}

// static
size_t NavigationControllerImpl::max_entry_count() {
  if (max_entry_count_for_testing_ != kMaxEntryCountForTestingNotSet)
    return max_entry_count_for_testing_;
  return blink::kMaxSessionHistoryEntries;
}

void NavigationControllerImpl::SetActive(bool is_active) {
  if (is_active && needs_reload_)
    LoadIfNecessary();
}

void NavigationControllerImpl::LoadIfNecessary() {
  if (!needs_reload_)
    return;

  UMA_HISTOGRAM_ENUMERATION("Navigation.LoadIfNecessaryType",
                            needs_reload_type_);

  // Calling Reload() results in ignoring state, and not loading.
  // Explicitly use NavigateToPendingEntry so that the renderer uses the
  // cached state.
  if (pending_entry_) {
    NavigateToExistingPendingEntry(ReloadType::NONE,
                                   FrameTreeNode::kFrameTreeNodeInvalidId,
                                   true /* is_browser_initiated */);
  } else if (last_committed_entry_index_ != -1) {
    pending_entry_ = entries_[last_committed_entry_index_].get();
    pending_entry_index_ = last_committed_entry_index_;
    NavigateToExistingPendingEntry(ReloadType::NONE,
                                   FrameTreeNode::kFrameTreeNodeInvalidId,
                                   true /* is_browser_initiated */);
  } else {
    // If there is something to reload, the successful reload will clear the
    // |needs_reload_| flag. Otherwise, just do it here.
    needs_reload_ = false;
  }
}

base::WeakPtr<NavigationHandle>
NavigationControllerImpl::LoadPostCommitErrorPage(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    const std::string& error_page_html,
    net::Error error) {
  RenderFrameHostImpl* rfhi =
      static_cast<RenderFrameHostImpl*>(render_frame_host);

  // Only active documents can load post-commit error pages:
  // - If the document is in pending deletion, the browser already committed to
  // destroying this RenderFrameHost so ignore loading the error page.
  // - If the document is in back-forward cache, it's not allowed to navigate as
  // it should remain frozen. Ignore the request and evict the document from
  // back-forward cache.
  // - If the document is prerendering, it can navigate but when loading error
  // pages, cancel prerendering.
  if (rfhi->IsInactiveAndDisallowActivation(
          DisallowActivationReasonId::kLoadPostCommitErrorPage)) {
    return nullptr;
  }

  FrameTreeNode* node = rfhi->frame_tree_node();

  blink::mojom::CommonNavigationParamsPtr common_params =
      blink::CreateCommonNavigationParams();
  // |url| might be empty, such as when LoadPostCommitErrorPage happens before
  // the frame actually committed (e.g. iframe with "src" set to a
  // slow-responding URL). We should rewrite the URL to about:blank in this
  // case, as the renderer will only think a page is an error page if it has a
  // non-empty unreachable URL.
  common_params->url = url.is_empty() ? GURL("about:blank") : url;
  blink::mojom::CommitNavigationParamsPtr commit_params =
      blink::CreateCommitNavigationParams();
  commit_params->original_url = common_params->url;

  // Error pages have a fully permissive FramePolicy.
  // TODO(arthursonzogni): Consider providing the minimal capabilities to the
  // error pages.
  commit_params->frame_policy = blink::FramePolicy();

  std::unique_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateBrowserInitiated(
          node, std::move(common_params), std::move(commit_params),
          true /* browser_initiated */, false /* was_opener_suppressed */,
          nullptr /* initiator_frame_token */,
          ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
          "" /* extra_headers */, nullptr /* frame_entry */,
          nullptr /* entry */, nullptr /* post_body */,
          nullptr /* navigation_ui_data */, absl::nullopt /* impression */,
          false /* is_pdf */);
  navigation_request->set_post_commit_error_page_html(error_page_html);
  navigation_request->set_net_error(error);
  node->CreatedNavigationRequest(std::move(navigation_request));
  DCHECK(node->navigation_request());

  // Calling BeginNavigation may destroy the NavigationRequest.
  base::WeakPtr<NavigationRequest> created_navigation_request(
      node->navigation_request()->GetWeakPtr());
  node->navigation_request()->BeginNavigation();
  return created_navigation_request;
}

void NavigationControllerImpl::NotifyEntryChanged(NavigationEntry* entry) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = GetIndexOfEntry(NavigationEntryImpl::FromNavigationEntry(entry));
  delegate_->NotifyNavigationEntryChanged(det);
}

void NavigationControllerImpl::FinishRestore(int selected_index,
                                             RestoreType type) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_, type);

  last_committed_entry_index_ = selected_index;
}

void NavigationControllerImpl::DiscardNonCommittedEntries() {
  // Avoid sending a notification if there is nothing to discard.
  // TODO(mthiesse): Temporarily checking failed_pending_entry_id_ to help
  // diagnose https://bugs.chromium.org/p/chromium/issues/detail?id=1007570.
  if (!pending_entry_ && failed_pending_entry_id_ == 0) {
    return;
  }
  DiscardPendingEntry(false);
  if (delegate_)
    delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
}

int NavigationControllerImpl::GetEntryIndexWithUniqueID(
    int nav_entry_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if (entries_[i]->GetUniqueID() == nav_entry_id)
      return i;
  }
  return -1;
}

void NavigationControllerImpl::InsertEntriesFrom(
    NavigationControllerImpl* source,
    int max_index) {
  DCHECK_LE(max_index, source->GetEntryCount());
  std::unique_ptr<NavigationEntryRestoreContextImpl> context =
      std::make_unique<NavigationEntryRestoreContextImpl>();
  for (int i = 0; i < max_index; i++) {
    // Normally, cloning a NavigationEntryImpl results in sharing
    // FrameNavigationEntries between the original and the clone. However, when
    // cloning from a different NavigationControllerImpl, we want to fork the
    // FrameNavigationEntries.
    entries_.insert(entries_.begin() + i,
                    source->entries_[i]->CloneWithoutSharing(context.get()));
  }
  DCHECK(pending_entry_index_ == -1 ||
         pending_entry_ == GetEntryAtIndex(pending_entry_index_));
}

void NavigationControllerImpl::SetGetTimestampCallbackForTest(
    const base::RepeatingCallback<base::Time()>& get_timestamp_callback) {
  get_timestamp_callback_ = get_timestamp_callback;
}

// History manipulation intervention:
void NavigationControllerImpl::SetShouldSkipOnBackForwardUIIfNeeded(
    bool replace_entry,
    bool previous_document_was_activated,
    bool is_renderer_initiated,
    ukm::SourceId previous_page_load_ukm_source_id) {
  // Note that for a subframe, previous_document_was_activated is true if the
  // gesture happened in any subframe (propagated to main frame) or in the main
  // frame itself.
  if (replace_entry || previous_document_was_activated ||
      !is_renderer_initiated) {
    return;
  }
  if (last_committed_entry_index_ == -1)
    return;

  SetSkippableForSameDocumentEntries(last_committed_entry_index_, true);

  // Log UKM with the URL we are navigating away from.
  ukm::builders::HistoryManipulationIntervention(
      previous_page_load_ukm_source_id)
      .Record(ukm::UkmRecorder::Get());
}

void NavigationControllerImpl::SetSkippableForSameDocumentEntries(
    int reference_index,
    bool skippable) {
  auto* reference_entry = GetEntryAtIndex(reference_index);
  reference_entry->set_should_skip_on_back_forward_ui(skippable);

  int64_t document_sequence_number =
      reference_entry->root_node()->frame_entry->document_sequence_number();
  for (int index = 0; index < GetEntryCount(); index++) {
    auto* entry = GetEntryAtIndex(index);
    if (entry->root_node()->frame_entry->document_sequence_number() ==
        document_sequence_number) {
      entry->set_should_skip_on_back_forward_ui(skippable);
    }
  }
}

std::unique_ptr<NavigationControllerImpl::PendingEntryRef>
NavigationControllerImpl::ReferencePendingEntry() {
  DCHECK(pending_entry_);
  auto pending_entry_ref =
      std::make_unique<PendingEntryRef>(weak_factory_.GetWeakPtr());
  pending_entry_refs_.insert(pending_entry_ref.get());
  return pending_entry_ref;
}

void NavigationControllerImpl::PendingEntryRefDeleted(PendingEntryRef* ref) {
  // Ignore refs that don't correspond to the current pending entry.
  auto it = pending_entry_refs_.find(ref);
  if (it == pending_entry_refs_.end())
    return;
  pending_entry_refs_.erase(it);

  if (!pending_entry_refs_.empty())
    return;

  // The pending entry may be deleted before the last PendingEntryRef.
  if (!pending_entry_)
    return;

  // We usually clear the pending entry when the matching NavigationRequest
  // fails, so that an arbitrary URL isn't left visible above a committed page.
  //
  // However, we do preserve the pending entry in some cases, such as on the
  // initial navigation of an unmodified blank tab. We also allow the delegate
  // to say when it's safe to leave aborted URLs in the omnibox, to let the
  // user edit the URL and try again. This may be useful in cases that the
  // committed page cannot be attacker-controlled. In these cases, we still
  // allow the view to clear the pending entry and typed URL if the user
  // requests (e.g., hitting Escape with focus in the address bar).
  //
  // Do not leave the pending entry visible if it has an invalid URL, since this
  // might be formatted in an unexpected or unsafe way.
  // TODO(creis): Block navigations to invalid URLs in https://crbug.com/850824.
  bool should_preserve_entry =
      (pending_entry_ == GetVisibleEntry()) &&
      pending_entry_->GetURL().is_valid() &&
      (IsUnmodifiedBlankTab() || delegate_->ShouldPreserveAbortedURLs());
  if (should_preserve_entry)
    return;

  DiscardPendingEntry(true);
  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
}

std::unique_ptr<PolicyContainerPolicies>
NavigationControllerImpl::ComputePolicyContainerPoliciesForFrameEntry(
    RenderFrameHostImpl* rfh,
    bool is_same_document,
    NavigationRequest* request) {
  if (!ShouldStorePolicyContainerPoliciesInFrameNavigationEntry(request))
    return nullptr;

  if (is_same_document) {
    // TODO(https://crbug.com/524208): Remove this nullptr check when we can
    // ensure we always have a FrameNavigationEntry here.
    if (!GetLastCommittedEntry())
      return nullptr;

    FrameNavigationEntry* previous_frame_entry =
        GetLastCommittedEntry()->GetFrameEntry(rfh->frame_tree_node());

    // TODO(https://crbug.com/608402): Remove this nullptr check when we can
    // ensure we always have a FrameNavigationEntry here.
    if (!previous_frame_entry)
      return nullptr;

    const PolicyContainerPolicies* previous_policies =
        previous_frame_entry->policy_container_policies();

    if (!previous_policies)
      return nullptr;

    // Make a copy of the policy container for the new FrameNavigationEntry.
    return previous_policies->Clone();
  }

  return rfh->policy_container_host()->policies().Clone();
}

void NavigationControllerImpl::BroadcastHistoryOffsetAndLength() {
  OPTIONAL_TRACE_EVENT2(
      "content", "NavigationControllerImpl::BroadcastHistoryOffsetAndLength",
      "history_offset", GetLastCommittedEntryIndex(), "history_length",
      GetEntryCount());

  auto callback = base::BindRepeating(
      [](int history_offset, int history_length, RenderViewHostImpl* rvh) {
        if (auto& broadcast = rvh->GetAssociatedPageBroadcast()) {
          broadcast->SetHistoryOffsetAndLength(history_offset, history_length);
        }
      },
      GetLastCommittedEntryIndex(), GetEntryCount());
  frame_tree_.root()->render_manager()->ExecutePageBroadcastMethod(callback);
}

void NavigationControllerImpl::DidAccessInitialMainDocument() {
  // We may have left a failed browser-initiated navigation in the address bar
  // to let the user edit it and try again.  Clear it now that content might
  // show up underneath it.
  if (!frame_tree_.IsLoading() && GetPendingEntry())
    DiscardPendingEntry(false);

  // Update the URL display.
  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
}

void NavigationControllerImpl::UpdateStateForFrame(
    RenderFrameHostImpl* rfhi,
    const blink::PageState& page_state) {
  OPTIONAL_TRACE_EVENT1("content",
                        "NavigationControllerImpl::UpdateStateForFrame",
                        "render_frame_host", rfhi);
  // The state update affects the last NavigationEntry associated with the given
  // |render_frame_host|. This may not be the last committed NavigationEntry (as
  // in the case of an UpdateState from a frame being swapped out). We track
  // which entry this is in the RenderFrameHost's nav_entry_id.
  NavigationEntryImpl* entry = GetEntryWithUniqueID(rfhi->nav_entry_id());
  if (!entry)
    return;

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(rfhi->frame_tree_node());
  if (!frame_entry)
    return;

  // The SiteInstance might not match if we do a cross-process navigation with
  // replacement (e.g., auto-subframe), in which case the swap out of the old
  // RenderFrameHost runs in the background after the old FrameNavigationEntry
  // has already been replaced and destroyed.
  if (frame_entry->site_instance() != rfhi->GetSiteInstance())
    return;

  if (page_state == frame_entry->page_state())
    return;  // Nothing to update.

  DCHECK(page_state.IsValid()) << "Shouldn't set an empty PageState.";

  // The document_sequence_number and item_sequence_number recorded in the
  // FrameNavigationEntry should not differ from the one coming with the update,
  // since it must come from the same document. Do not update it if a difference
  // is detected, as this indicates that |frame_entry| is not the correct one.
  blink::ExplodedPageState exploded_state;
  if (!blink::DecodePageState(page_state.ToEncodedData(), &exploded_state))
    return;

  if (exploded_state.top.document_sequence_number !=
          frame_entry->document_sequence_number() ||
      exploded_state.top.item_sequence_number !=
          frame_entry->item_sequence_number()) {
    return;
  }

  frame_entry->SetPageState(page_state);
  NotifyEntryChanged(entry);
}

void NavigationControllerImpl::OnStoragePartitionIdAdded(
    const StoragePartitionId& partition_id) {
  auto it = partition_config_to_id_map_.insert(
      std::make_pair(partition_id.config(), partition_id));
  bool successful_insert = it.second;
  if (!successful_insert) {
    LogStoragePartitionIdCrashKeys(it.first->second, partition_id);
  }
  CHECK(successful_insert);
}

void NavigationControllerImpl::LogStoragePartitionIdCrashKeys(
    const StoragePartitionId& original_partition_id,
    const StoragePartitionId& new_partition_id) {
  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("original_partition_id",
                                          base::debug::CrashKeySize::Size256),
      original_partition_id.ToString());

  base::debug::SetCrashKeyString(
      base::debug::AllocateCrashKeyString("new_partition_id",
                                          base::debug::CrashKeySize::Size256),
      new_partition_id.ToString());
}

std::vector<blink::mojom::AppHistoryEntryPtr>
NavigationControllerImpl::PopulateSingleAppHistoryEntryVector(
    Direction direction,
    int entry_index,
    const url::Origin& pending_origin,
    FrameTreeNode* node,
    SiteInstance* site_instance,
    int64_t previous_item_sequence_number) {
  std::vector<blink::mojom::AppHistoryEntryPtr> entries;
  int offset = direction == Direction::kForward ? 1 : -1;
  for (int i = entry_index + offset; i >= 0 && i < GetEntryCount();
       i += offset) {
    FrameNavigationEntry* frame_entry = GetEntryAtIndex(i)->GetFrameEntry(node);
    if (!frame_entry || !frame_entry->committed_origin())
      break;
    if (site_instance != frame_entry->site_instance())
      break;
    if (!pending_origin.IsSameOriginWith(*frame_entry->committed_origin()))
      break;
    if (previous_item_sequence_number == frame_entry->item_sequence_number())
      continue;
    blink::ExplodedPageState exploded_page_state;
    if (blink::DecodePageState(frame_entry->page_state().ToEncodedData(),
                               &exploded_page_state)) {
      blink::ExplodedFrameState frame_state = exploded_page_state.top;
      blink::mojom::AppHistoryEntryPtr entry =
          blink::mojom::AppHistoryEntry::New(
              frame_state.app_history_key.value_or(std::u16string()),
              frame_state.app_history_id.value_or(std::u16string()),
              frame_state.url_string.value_or(std::u16string()));
      DCHECK(pending_origin.CanBeDerivedFrom(GURL(entry->url)));
      entries.push_back(std::move(entry));
      previous_item_sequence_number = frame_entry->item_sequence_number();
    }
  }
  // If |entries| was constructed by iterating backwards from
  // |entry_index|, it's latest-at-the-front, but the renderer will want it
  // earliest-at-the-front. Reverse it.
  if (direction == Direction::kBack)
    std::reverse(entries.begin(), entries.end());
  return entries;
}

void NavigationControllerImpl::PopulateAppHistoryEntryVectors(
    NavigationRequest* request) {
  url::Origin pending_origin =
      request->commit_params().origin_to_commit
          ? *request->commit_params().origin_to_commit
          : url::Origin::Create(request->common_params().url);

  FrameTreeNode* node = request->frame_tree_node();
  scoped_refptr<SiteInstance> site_instance =
      request->GetRenderFrameHost()->GetSiteInstance();

  // NOTE: |entry_index| is an estimate of the index where this entry will
  // commit, but it may be wrong in corner cases (e.g., if we are at the max
  // entry limit, the earliest entry will be dropped). This is ok because this
  // algorithm only uses |entry_index| to walk the entry list as it stands right
  // now, and it isn't saved for anything post-commit.
  int entry_index = GetPendingEntryIndex();
  bool will_create_new_entry = false;
  if (NavigationTypeUtils::IsReload(request->common_params().navigation_type) ||
      request->common_params().should_replace_current_entry) {
    entry_index = GetLastCommittedEntryIndex();
  } else if (entry_index == -1) {
    will_create_new_entry = true;
    entry_index = GetLastCommittedEntryIndex() + 1;
  }

  int64_t pending_item_sequence_number = 0;
  if (auto* pending_entry = GetPendingEntry()) {
    if (auto* frame_entry = pending_entry->GetFrameEntry(node))
      pending_item_sequence_number = frame_entry->item_sequence_number();
  }

  request->set_app_history_back_entries(PopulateSingleAppHistoryEntryVector(
      Direction::kBack, entry_index, pending_origin, node, site_instance.get(),
      pending_item_sequence_number));

  // Don't populate forward entries if they will be truncated by a new entry.
  if (!will_create_new_entry) {
    request->set_app_history_forward_entries(
        PopulateSingleAppHistoryEntryVector(
            Direction::kForward, entry_index, pending_origin, node,
            site_instance.get(), pending_item_sequence_number));
  }
}

NavigationControllerImpl::HistoryNavigationAction
NavigationControllerImpl::ShouldNavigateToEntryForAppHistoryKey(
    FrameNavigationEntry* current_entry,
    FrameNavigationEntry* target_entry,
    const std::string& app_history_key) {
  if (!target_entry || !target_entry->committed_origin())
    return HistoryNavigationAction::kStopLooking;
  if (current_entry->site_instance() != target_entry->site_instance())
    return HistoryNavigationAction::kStopLooking;
  if (!current_entry->committed_origin()->IsSameOriginWith(
          *target_entry->committed_origin())) {
    return HistoryNavigationAction::kStopLooking;
  }

  // NOTE: We don't actually care between kSameDocument and
  // kDifferentDocument, so always use kDifferentDocument by convention.
  if (target_entry->app_history_key() == app_history_key)
    return HistoryNavigationAction::kDifferentDocument;
  return HistoryNavigationAction::kKeepLooking;
}

void NavigationControllerImpl::NavigateToAppHistoryKey(FrameTreeNode* node,
                                                       const std::string& key) {
  FrameNavigationEntry* current_entry =
      GetLastCommittedEntry()->GetFrameEntry(node);
  if (!current_entry)
    return;

  // We want to find the nearest matching entry that is contiguously
  // same-instance and same-origin. Check back first, then forward.
  // TODO(japhet): Link spec here once it exists.
  for (int i = GetCurrentEntryIndex() - 1; i >= 0; i--) {
    auto result = ShouldNavigateToEntryForAppHistoryKey(
        current_entry, GetEntryAtIndex(i)->GetFrameEntry(node), key);
    if (result == HistoryNavigationAction::kStopLooking)
      break;
    if (result != HistoryNavigationAction::kKeepLooking) {
      GoToIndex(i, FrameTreeNode::kFrameTreeNodeInvalidId,
                false /* is_browser_initiated*/);
      return;
    }
  }
  for (int i = GetCurrentEntryIndex() + 1; i < GetEntryCount(); i++) {
    auto result = ShouldNavigateToEntryForAppHistoryKey(
        current_entry, GetEntryAtIndex(i)->GetFrameEntry(node), key);
    if (result == HistoryNavigationAction::kStopLooking)
      break;
    if (result != HistoryNavigationAction::kKeepLooking) {
      GoToIndex(i, FrameTreeNode::kFrameTreeNodeInvalidId,
                false /* is_browser_initiated*/);
      return;
    }
  }
}

bool NavigationControllerImpl::ShouldMaintainTrivialSessionHistory() const {
  // TODO(https://crbug.com/1197384): We may have to add portals and fenced
  // frames in addition to prerender. This should be kept in sync with
  // LocalFrame version, LocalFrame::ShouldMaintainTrivialSessionHistory.
  return frame_tree_.is_prerendering();
}

}  // namespace content
