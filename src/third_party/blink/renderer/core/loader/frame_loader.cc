/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) Research In Motion Limited 2009. All rights reserved.
 * Copyright (C) 2011 Kris Jordan <krisjordan@gmail.com>
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/loader/frame_loader.h"

#include <memory>
#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/unguessable_token.h"
#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom-shared.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/public/mojom/frame/navigation_initiator.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_mixed_content.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/ignore_opens_during_unload_count_incrementer.h"
#include "third_party/blink/renderer/core/events/page_transition_event.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
#include "third_party/blink/renderer/core/loader/document_load_timing.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/form_submission.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/loader/navigation_scheduler.h"
#include "third_party/blink/renderer/core/loader/progress_tracker.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/frame_tree.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scroll_animator_base.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/microtask.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_activity_logger.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using namespace html_names;

bool IsBackForwardLoadType(WebFrameLoadType type) {
  return type == WebFrameLoadType::kBackForward;
}

bool IsReloadLoadType(WebFrameLoadType type) {
  return type == WebFrameLoadType::kReload ||
         type == WebFrameLoadType::kReloadBypassingCache;
}

static bool NeedsHistoryItemRestore(WebFrameLoadType type) {
  return type == WebFrameLoadType::kBackForward || IsReloadLoadType(type);
}

static SinglePageAppNavigationType CategorizeSinglePageAppNavigation(
    SameDocumentNavigationSource same_document_navigation_source,
    WebFrameLoadType frame_load_type) {
  // |SinglePageAppNavigationType| falls into this grid according to different
  // combinations of |WebFrameLoadType| and |SameDocumentNavigationSource|:
  //
  //                 HistoryApi           Default
  //  kBackForward   illegal              otherFragmentNav
  // !kBackForward   sameDocBack/Forward  historyPushOrReplace
  switch (same_document_navigation_source) {
    case kSameDocumentNavigationDefault:
      if (frame_load_type == WebFrameLoadType::kBackForward) {
        return kSPANavTypeSameDocumentBackwardOrForward;
      }
      return kSPANavTypeOtherFragmentNavigation;
    case kSameDocumentNavigationHistoryApi:
      // It's illegal to have both kSameDocumentNavigationHistoryApi and
      // WebFrameLoadType::kBackForward.
      DCHECK(frame_load_type != WebFrameLoadType::kBackForward);
      return kSPANavTypeHistoryPushStateOrReplaceState;
  }
  NOTREACHED();
  return kSPANavTypeSameDocumentBackwardOrForward;
}

ResourceRequest FrameLoader::ResourceRequestForReload(
    WebFrameLoadType frame_load_type,
    ClientRedirectPolicy client_redirect_policy) {
  DCHECK(IsReloadLoadType(frame_load_type));
  const auto cache_mode =
      frame_load_type == WebFrameLoadType::kReloadBypassingCache
          ? mojom::FetchCacheMode::kBypassCache
          : mojom::FetchCacheMode::kValidateCache;
  if (!document_loader_ || !document_loader_->GetHistoryItem())
    return ResourceRequest();

  ResourceRequest request =
      document_loader_->GetHistoryItem()->GenerateResourceRequest(cache_mode);
  request.SetRequestorOrigin(frame_->GetSecurityContext()->GetSecurityOrigin());

  // ClientRedirectPolicy is an indication that this load was triggered by some
  // direct interaction with the page. If this reload is not a client redirect,
  // we should reuse the referrer from the original load of the current
  // document. If this reload is a client redirect (e.g., location.reload()), it
  // was initiated by something in the current document and should therefore
  // show the current document's url as the referrer.
  // TODO(domfarolino): Stop storing ResourceRequest's generated referrer as a
  // header and instead use a separate member. See https://crbug.com/850813.
  if (client_redirect_policy == ClientRedirectPolicy::kClientRedirect) {
    request.SetHttpReferrer(SecurityPolicy::GenerateReferrer(
        frame_->GetDocument()->GetReferrerPolicy(),
        frame_->GetDocument()->Url(),
        frame_->GetDocument()->OutgoingReferrer()));
  }

  request.SetSkipServiceWorker(frame_load_type ==
                               WebFrameLoadType::kReloadBypassingCache);
  return request;
}

FrameLoader::FrameLoader(LocalFrame* frame)
    : frame_(frame),
      progress_tracker_(MakeGarbageCollected<ProgressTracker>(frame)),
      in_restore_scroll_(false),
      forced_sandbox_flags_(WebSandboxFlags::kNone),
      dispatching_did_clear_window_object_in_main_world_(false),
      detached_(false),
      virtual_time_pauser_(
          frame_->GetFrameScheduler()->CreateWebScopedVirtualTimePauser(
              "FrameLoader",
              WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant)) {
  DCHECK(frame_);

  TRACE_EVENT_OBJECT_CREATED_WITH_ID("loading", "FrameLoader", this);
  TakeObjectSnapshot();
}

FrameLoader::~FrameLoader() {
  DCHECK(detached_);
}

void FrameLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  visitor->Trace(progress_tracker_);
  visitor->Trace(document_loader_);
  visitor->Trace(provisional_document_loader_);
  visitor->Trace(last_origin_document_csp_);
}

void FrameLoader::Init() {
  ScriptForbiddenScope forbid_scripts;

  auto navigation_params = std::make_unique<WebNavigationParams>();
  navigation_params->url = KURL(g_empty_string);
  provisional_document_loader_ = Client()->CreateDocumentLoader(
      frame_, kWebNavigationTypeOther, std::move(navigation_params),
      nullptr /* extra_data */);
  bool success = provisional_document_loader_->PrepareForLoad();
  DCHECK(success);
  provisional_document_loader_->StartLoading();

  frame_->GetDocument()->CancelParsing();

  state_machine_.AdvanceTo(
      FrameLoaderStateMachine::kDisplayingInitialEmptyDocument);

  // Suppress finish notifications for initial empty documents, since they don't
  // generate start notifications.
  document_loader_->SetSentDidFinishLoad();
  if (frame_->GetPage()->Paused())
    frame_->SetLifecycleState(mojom::FrameLifecycleState::kPaused);

  TakeObjectSnapshot();
}

LocalFrameClient* FrameLoader::Client() const {
  return frame_->Client();
}

void FrameLoader::SetDefersLoading(bool defers) {
  if (frame_->GetDocument())
    frame_->GetDocument()->Fetcher()->SetDefersLoading(defers);
  if (document_loader_)
    document_loader_->SetDefersLoading(defers);
  if (provisional_document_loader_)
    provisional_document_loader_->SetDefersLoading(defers);
  if (!defers)
    frame_->GetNavigationScheduler().StartTimer();
}

void FrameLoader::SaveScrollAnchor() {
  if (!RuntimeEnabledFeatures::ScrollAnchorSerializationEnabled())
    return;

  if (!document_loader_ || !document_loader_->GetHistoryItem() ||
      !frame_->View())
    return;

  // Shouldn't clobber anything if we might still restore later.
  if (NeedsHistoryItemRestore(document_loader_->LoadType()) &&
      !document_loader_->GetInitialScrollState().was_scrolled_by_user)
    return;

  HistoryItem* history_item = document_loader_->GetHistoryItem();
  if (ScrollableArea* layout_scrollable_area =
          frame_->View()->LayoutViewport()) {
    ScrollAnchor* scroll_anchor = layout_scrollable_area->GetScrollAnchor();
    DCHECK(scroll_anchor);

    const SerializedAnchor& serialized_anchor =
        scroll_anchor->GetSerializedAnchor();
    if (serialized_anchor.IsValid()) {
      history_item->SetScrollAnchorData(
          {serialized_anchor.selector,
           WebFloatPoint(serialized_anchor.relative_offset.X(),
                         serialized_anchor.relative_offset.Y()),
           serialized_anchor.simhash});
    }
  }
}

void FrameLoader::SaveScrollState() {
  if (!document_loader_ || !document_loader_->GetHistoryItem() ||
      !frame_->View())
    return;

  // Shouldn't clobber anything if we might still restore later.
  if (NeedsHistoryItemRestore(document_loader_->LoadType()) &&
      !document_loader_->GetInitialScrollState().was_scrolled_by_user)
    return;

  HistoryItem* history_item = document_loader_->GetHistoryItem();
  // For performance reasons, we don't save scroll anchors as often as we save
  // scroll offsets. In order to avoid keeping around a stale anchor, we clear
  // it when the saved scroll offset changes.
  history_item->SetScrollAnchorData(ScrollAnchorData());
  if (ScrollableArea* layout_scrollable_area = frame_->View()->LayoutViewport())
    history_item->SetScrollOffset(layout_scrollable_area->GetScrollOffset());
  history_item->SetVisualViewportScrollOffset(ToScrollOffset(
      frame_->GetPage()->GetVisualViewport().VisibleRect().Location()));

  if (frame_->IsMainFrame())
    history_item->SetPageScaleFactor(frame_->GetPage()->PageScaleFactor());

  Client()->DidUpdateCurrentHistoryItem();
}

void FrameLoader::DispatchUnloadEvent() {
  FrameNavigationDisabler navigation_disabler(*frame_);
  SaveScrollState();

  Document* document = frame_->GetDocument();
  if (document && !SVGImage::IsInSVGImage(document)) {
    document->DispatchUnloadEvents(
        provisional_document_loader_
            ? &provisional_document_loader_->GetTiming()
            : nullptr);
    // Don't remove event listeners from a transitional empty document (see
    // https://bugs.webkit.org/show_bug.cgi?id=28716 for more information).
    bool keep_event_listeners =
        provisional_document_loader_ &&
        ShouldReuseDefaultView(
            provisional_document_loader_->Url(),
            provisional_document_loader_->GetContentSecurityPolicy());
    if (!keep_event_listeners)
      document->RemoveAllEventListenersRecursively();
  }
}

void FrameLoader::DidExplicitOpen() {
  probe::LifecycleEvent(frame_, GetDocumentLoader(), "init",
                        CurrentTimeTicksInSeconds());
  // Calling document.open counts as committing the first real document load.
  if (!state_machine_.CommittedFirstRealDocumentLoad())
    state_machine_.AdvanceTo(FrameLoaderStateMachine::kCommittedFirstRealLoad);

  // Only model a document.open() as part of a navigation if its parent is not
  // done or in the process of completing.
  if (Frame* parent = frame_->Tree().Parent()) {
    auto* parent_local_frame = DynamicTo<LocalFrame>(parent);
    if ((parent_local_frame &&
         parent_local_frame->GetDocument()->LoadEventStillNeeded()) ||
        (parent->IsRemoteFrame() && parent->IsLoading())) {
      progress_tracker_->ProgressStarted();
    }
  }
}

// This is only called by ScriptController::executeScriptIfJavaScriptURL and
// always contains the result of evaluating a javascript: url. This is the
// <iframe src="javascript:'html'"> case.
void FrameLoader::ReplaceDocumentWhileExecutingJavaScriptURL(
    const String& source,
    Document* owner_document) {
  Document* document = frame_->GetDocument();
  if (!document_loader_ ||
      document->PageDismissalEventBeingDispatched() != Document::kNoDismissal)
    return;

  UseCounter::Count(*document, WebFeature::kReplaceDocumentViaJavaScriptURL);

  const KURL& url = document->Url();

  // Compute this before clearing the frame, because it may need to inherit an
  // aliased security context.
  // The document CSP is the correct one as it is used for CSP checks
  // done previously before getting here:
  // HTMLFormElement::ScheduleFormSubmission
  // HTMLFrameElementBase::OpenURL
  GlobalObjectReusePolicy global_object_reuse_policy =
      ShouldReuseDefaultView(url, document->GetContentSecurityPolicy())
          ? GlobalObjectReusePolicy::kUseExisting
          : GlobalObjectReusePolicy::kCreateNew;

  document_loader_->StopLoading();

  // Don't allow any new child frames to load in this frame: attaching a new
  // child frame during or after detaching children results in an attached
  // frame on a detached DOM tree, which is bad.
  SubframeLoadingDisabler disabler(document);
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of the parent Document must be
  // incremented when unloading its descendants.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(document);
  frame_->DetachChildren();

  // detachChildren() potentially detaches or navigates this frame. The load
  // cannot continue in those cases.
  if (!frame_->IsAttached() || document != frame_->GetDocument())
    return;

  frame_->GetDocument()->Shutdown();
  Client()->TransitionToCommittedForNewPage();
  document_loader_->ReplaceDocumentWhileExecutingJavaScriptURL(
      url, owner_document, global_object_reuse_policy, source);
}

void FrameLoader::FinishedParsing() {
  if (state_machine_.CreatingInitialEmptyDocument())
    return;

  progress_tracker_->FinishedParsing();

  if (Client()) {
    ScriptForbiddenScope forbid_scripts;
    Client()->DispatchDidFinishDocumentLoad();
  }

  if (Client()) {
    Client()->RunScriptsAtDocumentReady(
        document_loader_ ? document_loader_->IsCommittedButEmpty() : true);
  }

  if (frame_->View()) {
    ProcessFragment(frame_->GetDocument()->Url(), document_loader_->LoadType(),
                    kNavigationToDifferentDocument);
  }

  frame_->GetDocument()->CheckCompleted();
}

bool FrameLoader::AllAncestorsAreComplete() const {
  for (Frame* ancestor = frame_; ancestor;
       ancestor = ancestor->Tree().Parent()) {
    if (ancestor->IsLoading())
      return false;
  }
  return true;
}

void FrameLoader::DidFinishNavigation() {
  // We should have either finished the provisional or committed navigation if
  // this is called. Only delcare the whole frame finished if neither is in
  // progress.
  DCHECK((document_loader_ && document_loader_->SentDidFinishLoad()) ||
         !HasProvisionalNavigation());
  if ((document_loader_ && !document_loader_->SentDidFinishLoad()) ||
      HasProvisionalNavigation()) {
    return;
  }

  // This code in this block is meant to prepare a document for display, but
  // this code may also run when swapping out a provisional frame. In that case,
  // skip the display work.
  if (frame_->IsLoading() && !frame_->IsProvisional()) {
    progress_tracker_->ProgressCompleted();
    // Retry restoring scroll offset since finishing loading disables content
    // size clamping.
    RestoreScrollPositionAndViewState();
    if (document_loader_)
      document_loader_->SetLoadType(WebFrameLoadType::kStandard);
    frame_->DomWindow()->FinishedLoading();
  }

  Frame* parent = frame_->Tree().Parent();
  if (parent)
    parent->CheckCompleted();
}

Frame* FrameLoader::Opener() {
  return Client() ? Client()->Opener() : nullptr;
}

void FrameLoader::SetOpener(LocalFrame* opener) {
  // If the frame is already detached, the opener has already been cleared.
  if (Client())
    Client()->SetOpener(opener);
}

bool FrameLoader::AllowPlugins(ReasonForCallingAllowPlugins reason) {
  // With Oilpan, a FrameLoader might be accessed after the Page has been
  // detached. FrameClient will not be accessible, so bail early.
  if (!Client())
    return false;
  Settings* settings = frame_->GetSettings();
  bool allowed = settings && settings->GetPluginsEnabled();
  if (!allowed && reason == kAboutToInstantiatePlugin) {
    if (auto* settings_client = frame_->GetContentSettingsClient())
      settings_client->DidNotAllowPlugins();
  }
  return allowed;
}

void FrameLoader::UpdateForSameDocumentNavigation(
    const KURL& new_url,
    SameDocumentNavigationSource same_document_navigation_source,
    scoped_refptr<SerializedScriptValue> data,
    HistoryScrollRestorationType scroll_restoration_type,
    WebFrameLoadType type,
    Document* initiating_document) {
  SinglePageAppNavigationType single_page_app_navigation_type =
      CategorizeSinglePageAppNavigation(same_document_navigation_source, type);
  UMA_HISTOGRAM_ENUMERATION(
      "RendererScheduler.UpdateForSameDocumentNavigationCount",
      single_page_app_navigation_type, kSPANavTypeCount);

  TRACE_EVENT1("blink", "FrameLoader::updateForSameDocumentNavigation", "url",
               new_url.GetString().Ascii().data());

  // Generate start and stop notifications only when loader is completed so that
  // we don't fire them for fragment redirection that happens in window.onload
  // handler. See https://bugs.webkit.org/show_bug.cgi?id=31838
  // Do not fire the notifications if the frame is concurrently navigating away
  // from the document, since a new document is already loading.
  bool was_loading = frame_->IsLoading();
  if (!was_loading)
    Client()->DidStartLoading();

  // Update the data source's request with the new URL to fake the URL change
  frame_->GetDocument()->SetURL(new_url);
  GetDocumentLoader()->UpdateForSameDocumentNavigation(
      new_url, same_document_navigation_source, std::move(data),
      scroll_restoration_type, type, initiating_document);
  if (!was_loading)
    Client()->DidStopLoading();
}

void FrameLoader::DetachDocumentLoader(Member<DocumentLoader>& loader,
                                       bool flush_microtask_queue) {
  if (!loader)
    return;

  if (loader == provisional_document_loader_)
    virtual_time_pauser_.UnpauseVirtualTime();

  FrameNavigationDisabler navigation_disabler(*frame_);
  loader->DetachFromFrame(flush_microtask_queue);
  loader = nullptr;
}

void FrameLoader::LoadInSameDocument(
    const KURL& url,
    scoped_refptr<SerializedScriptValue> state_object,
    WebFrameLoadType frame_load_type,
    HistoryItem* history_item,
    ClientRedirectPolicy client_redirect,
    Document* initiating_document,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  // If we have a state object, we cannot also be a new navigation.
  DCHECK(!state_object || frame_load_type == WebFrameLoadType::kBackForward);

  // If we have a provisional request for a different document, a fragment
  // scroll should cancel it.
  DetachDocumentLoader(provisional_document_loader_);

  if (!frame_->GetPage())
    return;
  SaveScrollState();

  KURL old_url = frame_->GetDocument()->Url();
  bool hash_change = EqualIgnoringFragmentIdentifier(url, old_url) &&
                     url.FragmentIdentifier() != old_url.FragmentIdentifier();
  if (hash_change) {
    // If we were in the autoscroll/middleClickAutoscroll mode we want to stop
    // it before following the link to the anchor
    frame_->GetEventHandler().StopAutoscroll();
    frame_->DomWindow()->EnqueueHashchangeEvent(old_url, url);
  }
  document_loader_->SetIsClientRedirect(client_redirect ==
                                        ClientRedirectPolicy::kClientRedirect);
  if (history_item)
    document_loader_->SetItemForHistoryNavigation(history_item);
  if (extra_data)
    Client()->UpdateDocumentLoader(document_loader_, std::move(extra_data));
  UpdateForSameDocumentNavigation(url, kSameDocumentNavigationDefault, nullptr,
                                  kScrollRestorationAuto, frame_load_type,
                                  initiating_document);

  document_loader_->GetInitialScrollState().was_scrolled_by_user = false;

  frame_->GetDocument()->CheckCompleted();

  // onpopstate might change view state, so stash for later restore.
  base::Optional<HistoryItem::ViewState> view_state;
  if (history_item) {
    view_state = history_item->GetViewState();
  }

  frame_->DomWindow()->StatePopped(state_object
                                       ? std::move(state_object)
                                       : SerializedScriptValue::NullValue());

  if (view_state) {
    RestoreScrollPositionAndViewState(frame_load_type,
                                      true /* is_same_document */, *view_state,
                                      history_item->ScrollRestorationType());
  }

  // We need to scroll to the fragment whether or not a hash change occurred,
  // since the user might have scrolled since the previous navigation.
  ProcessFragment(url, frame_load_type, kNavigationWithinSameDocument);

  TakeObjectSnapshot();
}

// static
void FrameLoader::SetReferrerForFrameRequest(FrameLoadRequest& frame_request) {
  ResourceRequest& request = frame_request.GetResourceRequest();
  Document* origin_document = frame_request.OriginDocument();

  if (!origin_document)
    return;
  if (frame_request.GetShouldSendReferrer() == kNeverSendReferrer)
    return;

  // Always use the initiating document to generate the referrer. We need to
  // generateReferrer(), because we haven't enforced
  // network::mojom::ReferrerPolicy or https->http referrer suppression yet.
  String referrer_to_use = request.ReferrerString();
  network::mojom::ReferrerPolicy referrer_policy_to_use =
      request.GetReferrerPolicy();

  if (referrer_to_use == Referrer::ClientReferrerString())
    referrer_to_use = origin_document->OutgoingReferrer();

  if (referrer_policy_to_use == network::mojom::ReferrerPolicy::kDefault)
    referrer_policy_to_use = origin_document->GetReferrerPolicy();

  Referrer referrer = SecurityPolicy::GenerateReferrer(
      referrer_policy_to_use, request.Url(), referrer_to_use);

  // TODO(domfarolino): Stop storing ResourceRequest's generated referrer as a
  // header and instead use a separate member. See https://crbug.com/850813.
  request.SetHttpReferrer(referrer);
  request.SetHTTPOriginToMatchReferrerIfNeeded();
}

WebFrameLoadType FrameLoader::DetermineFrameLoadType(
    const KURL& url,
    const AtomicString& http_method,
    Document* origin_document,
    const KURL& failing_url,
    WebFrameLoadType frame_load_type) {
  // TODO(dgozman): this method is rewriting the load type, which makes it hard
  // to reason about various navigations and their desired load type. We should
  // untangle it and detect the load type at the proper place. See, for example,
  // location.assign() block below.
  // Achieving that is complicated due to similar conditions in many places
  // both in the renderer and in the browser.
  if (frame_load_type == WebFrameLoadType::kStandard ||
      frame_load_type == WebFrameLoadType::kReplaceCurrentItem) {
    if (frame_->Tree().Parent() &&
        !state_machine_.CommittedFirstRealDocumentLoad())
      return WebFrameLoadType::kReplaceCurrentItem;
    if (!frame_->Tree().Parent() && !Client()->BackForwardLength()) {
      if (Opener() && url.IsEmpty())
        return WebFrameLoadType::kReplaceCurrentItem;
      return WebFrameLoadType::kStandard;
    }
  }
  if (frame_load_type != WebFrameLoadType::kStandard)
    return frame_load_type;
  // From the HTML5 spec for location.assign():
  // "If the browsing context's session history contains only one Document,
  // and that was the about:blank Document created when the browsing context
  // was created, then the navigation must be done with replacement enabled."
  if ((!state_machine_.CommittedMultipleRealLoads() &&
       DeprecatedEqualIgnoringCase(frame_->GetDocument()->Url(), BlankURL())))
    return WebFrameLoadType::kReplaceCurrentItem;

  if (url == document_loader_->UrlForHistory()) {
    if (http_method == http_names::kPOST)
      return WebFrameLoadType::kStandard;
    if (!origin_document)
      return WebFrameLoadType::kReload;
    return WebFrameLoadType::kReplaceCurrentItem;
  }

  if (failing_url == document_loader_->UrlForHistory() &&
      document_loader_->LoadType() == WebFrameLoadType::kReload)
    return WebFrameLoadType::kReload;

  if (url.IsEmpty() && failing_url.IsEmpty()) {
    return WebFrameLoadType::kReplaceCurrentItem;
  }

  if (origin_document && !origin_document->CanCreateHistoryEntry())
    return WebFrameLoadType::kReplaceCurrentItem;

  return WebFrameLoadType::kStandard;
}

bool FrameLoader::PrepareRequestForThisFrame(FrameLoadRequest& request) {
  // If no origin Document* was specified, skip remaining security checks and
  // assume the caller has fully initialized the FrameLoadRequest.
  if (!request.OriginDocument())
    return true;

  KURL url = request.GetResourceRequest().Url();
  if (url.ProtocolIsJavaScript()) {
    Document* origin_document = request.OriginDocument();
    // Check the CSP of the caller (the "source browsing context") if required,
    // as per https://html.spec.whatwg.org/C/#javascript-protocol.
    bool javascript_url_is_allowed =
        request.ShouldCheckMainWorldContentSecurityPolicy() ==
            kDoNotCheckContentSecurityPolicy ||
        origin_document->GetContentSecurityPolicy()->AllowInline(
            ContentSecurityPolicy::InlineType::kNavigation,
            frame_->DeprecatedLocalOwner(), url.GetString(),
            String() /* nonce */, origin_document->Url(),
            OrdinalNumber::First());

    if (!javascript_url_is_allowed)
      return false;

    if (frame_->Owner() &&
        ((frame_->Owner()->GetFramePolicy().sandbox_flags &
          WebSandboxFlags::kOrigin) != WebSandboxFlags::kNone))
      return false;

    frame_->GetDocument()->ProcessJavaScriptUrl(
        url, request.ShouldCheckMainWorldContentSecurityPolicy());
    return false;
  }

  if (!request.OriginDocument()->GetSecurityOrigin()->CanDisplay(url)) {
    request.OriginDocument()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Not allowed to load local resource: " + url.ElidedString()));
    return false;
  }
  return true;
}

static WebNavigationType DetermineNavigationType(
    WebFrameLoadType frame_load_type,
    bool is_form_submission,
    bool have_event) {
  bool is_reload = IsReloadLoadType(frame_load_type);
  bool is_back_forward = IsBackForwardLoadType(frame_load_type);
  if (is_form_submission) {
    return (is_reload || is_back_forward) ? kWebNavigationTypeFormResubmitted
                                          : kWebNavigationTypeFormSubmitted;
  }
  if (have_event)
    return kWebNavigationTypeLinkClicked;
  if (is_reload)
    return kWebNavigationTypeReload;
  if (is_back_forward)
    return kWebNavigationTypeBackForward;
  return kWebNavigationTypeOther;
}

static mojom::RequestContextType DetermineRequestContextFromNavigationType(
    const WebNavigationType navigation_type) {
  switch (navigation_type) {
    case kWebNavigationTypeLinkClicked:
      return mojom::RequestContextType::HYPERLINK;

    case kWebNavigationTypeOther:
      return mojom::RequestContextType::LOCATION;

    case kWebNavigationTypeFormResubmitted:
    case kWebNavigationTypeFormSubmitted:
      return mojom::RequestContextType::FORM;

    case kWebNavigationTypeBackForward:
    case kWebNavigationTypeReload:
      return mojom::RequestContextType::INTERNAL;
  }
  NOTREACHED();
  return mojom::RequestContextType::HYPERLINK;
}

void FrameLoader::StartNavigation(const FrameLoadRequest& passed_request,
                                  WebFrameLoadType frame_load_type) {
  CHECK(!IsBackForwardLoadType(frame_load_type));
  DCHECK(passed_request.TriggeringEventInfo() !=
         WebTriggeringEventInfo::kUnknown);

  DCHECK(frame_->GetDocument());
  if (HTMLFrameOwnerElement* element = frame_->DeprecatedLocalOwner())
    element->CancelPendingLazyLoad();

  FrameLoadRequest request(passed_request);
  ResourceRequest& resource_request = request.GetResourceRequest();
  const KURL& url = resource_request.Url();
  Document* origin_document = request.OriginDocument();

  resource_request.SetHasUserGesture(
      LocalFrame::HasTransientUserActivation(frame_));

  if (!PrepareRequestForThisFrame(request))
    return;

  SetReferrerForFrameRequest(request);

  // A GetNavigationPolicy() value other than kNavigationPolicyCurrentTab at
  // this point indicates that a user event modified the navigation policy
  // (e.g., a ctrl-click). Let the user's action override any target attribute.
  if (request.GetNavigationPolicy() == kNavigationPolicyCurrentTab) {
    Frame* target_frame =
        frame_->Tree().FindOrCreateFrameForNavigation(request).frame;
    request.SetNavigationPolicy(kNavigationPolicyCurrentTab);
    if (!target_frame)
      return;
    if (target_frame != frame_) {
      bool was_in_same_page = target_frame->GetPage() == frame_->GetPage();
      request.SetFrameName("_self");
      target_frame->Navigate(request, frame_load_type);
      Page* page = target_frame->GetPage();
      if (!was_in_same_page && page)
        page->GetChromeClient().Focus(frame_);
      return;
    }
  }

  // Block renderer-initiated loads of data: and filesystem: URLs in the top
  // frame.
  //
  // If the mime type of the data URL is supported, the URL will
  // eventually be rendered, so block it here. Otherwise, the load might be
  // handled by a plugin or end up as a download, so allow it to let the
  // embedder figure out what to do with it. Navigations to filesystem URLs are
  // always blocked here.
  if (frame_->IsMainFrame() && origin_document &&
      !frame_->Client()->AllowContentInitiatedDataUrlNavigations(
          request.OriginDocument()->Url()) &&
      (url.ProtocolIs("filesystem") ||
       (url.ProtocolIsData() &&
        network_utils::IsDataURLMimeTypeSupported(url)))) {
    frame_->GetDocument()->AddConsoleMessage(ConsoleMessage::Create(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Not allowed to navigate top frame to " + url.Protocol() +
            " URL: " + url.ElidedString()));
    return;
  }

  // TODO(dgozman): merge page dismissal check and FrameNavigationDisabler.
  if (!frame_->IsNavigationAllowed() ||
      frame_->GetDocument()->PageDismissalEventBeingDispatched() !=
          Document::kNoDismissal) {
    return;
  }

  frame_load_type = DetermineFrameLoadType(
      resource_request.Url(), resource_request.HttpMethod(), origin_document,
      KURL(), frame_load_type);

  bool same_document_navigation =
      request.GetNavigationPolicy() == kNavigationPolicyCurrentTab &&
      ShouldPerformFragmentNavigation(
          request.Form(), resource_request.HttpMethod(), frame_load_type, url);

  // Perform same document navigation.
  if (same_document_navigation) {
    CommitSameDocumentNavigation(
        url, frame_load_type, nullptr, request.ClientRedirect(),
        origin_document,
        request.TriggeringEventInfo() != WebTriggeringEventInfo::kNotFromEvent,
        nullptr /* extra_data */);
    return;
  }

  WebNavigationType navigation_type = DetermineNavigationType(
      frame_load_type, resource_request.HttpBody() || request.Form(),
      request.TriggeringEventInfo() != WebTriggeringEventInfo::kNotFromEvent);
  resource_request.SetRequestContext(
      DetermineRequestContextFromNavigationType(navigation_type));
  request.SetFrameType(frame_->IsMainFrame()
                           ? network::mojom::RequestContextFrameType::kTopLevel
                           : network::mojom::RequestContextFrameType::kNested);

  mojom::blink::NavigationInitiatorPtr navigation_initiator;
  WebContentSecurityPolicyList initiator_csp;
  if (origin_document && origin_document->GetContentSecurityPolicy()
                             ->ExperimentalFeaturesEnabled()) {
    initiator_csp = origin_document->GetContentSecurityPolicy()
                        ->ExposeForNavigationalChecks();
    auto mojo_request = mojo::MakeRequest(&navigation_initiator);
    origin_document->BindNavigationInitiatorRequest(std::move(mojo_request));
  }

  if (origin_document && origin_document->GetContentSecurityPolicy()) {
    last_origin_document_csp_ = MakeGarbageCollected<ContentSecurityPolicy>();
    last_origin_document_csp_->CopyStateFrom(
        origin_document->GetContentSecurityPolicy());
    last_origin_document_csp_->CopyPluginTypesFrom(
        origin_document->GetContentSecurityPolicy());
  }

  // Record the latest requiredCSP value that will be used when sending this
  // request.
  RecordLatestRequiredCSP();

  // TODO(arthursonzogni): 'frame-src' check is disabled on the
  // renderer side, but is enforced on the browser side.
  // See http://crbug.com/692595 for understanding why it
  // can't be enforced on both sides instead.

  // 'form-action' check in the frame that is navigating is disabled on the
  // renderer side, but is enforced on the browser side instead.
  // N.B. check in the frame that initiates the navigation stills occurs in
  // blink and is not enforced on the browser-side.
  // TODO(arthursonzogni) The 'form-action' check should be fully disabled
  // in blink, except when the form submission doesn't trigger a navigation
  // (i.e. javascript urls). Please see https://crbug.com/701749.

  // Report-only CSP headers are checked in browser.
  ModifyRequestForCSP(resource_request, origin_document,
                      request.GetFrameType());

  DCHECK(Client()->HasWebView());
  // Check for non-escaped new lines in the url.
  if (url.PotentiallyDanglingMarkup() && url.ProtocolIsInHTTPFamily()) {
    Deprecation::CountDeprecation(
        origin_document, WebFeature::kCanRequestURLHTTPContainingNewline);
    return;
  }

  bool has_transient_activation =
      LocalFrame::HasTransientUserActivation(frame_);
  // TODO(csharrison): In M71 when UserActivation v2 should ship, we can remove
  // the check that the pages are equal, because consumption should not be
  // shared across pages. After that, we can also get rid of consumption call
  // in RenderFrameImpl::OpenURL.
  if (frame_->IsMainFrame() && origin_document &&
      frame_->GetPage() == origin_document->GetPage()) {
    LocalFrame::ConsumeTransientUserActivation(frame_);
  }

  // The main resource request gets logged here, because V8DOMActivityLogger
  // is looked up based on the current v8::Context. When the request actually
  // begins, the v8::Context may no longer be on the stack.
  if (V8DOMActivityLogger* activity_logger =
          V8DOMActivityLogger::CurrentActivityLoggerIfIsolatedWorld()) {
    if (!DocumentLoader::WillLoadUrlAsEmpty(url)) {
      Vector<String> argv;
      argv.push_back("Main resource");
      argv.push_back(url.GetString());
      activity_logger->LogEvent("blinkRequestResource", argv.size(),
                                argv.data());
    }
  }

  if (request.ClientRedirectReason() != ClientNavigationReason::kNone) {
    probe::FrameRequestedNavigation(frame_, frame_, url,
                                    request.ClientRedirectReason());
  }

  Client()->BeginNavigation(
      resource_request, request.GetFrameType(), origin_document,
      nullptr /* document_loader */, navigation_type,
      request.GetNavigationPolicy(), has_transient_activation, frame_load_type,
      request.ClientRedirect() == ClientRedirectPolicy::kClientRedirect,
      request.TriggeringEventInfo(), request.Form(),
      request.ShouldCheckMainWorldContentSecurityPolicy(),
      request.GetBlobURLToken(), request.GetInputStartTime(),
      request.HrefTranslate().GetString(), std::move(initiator_csp),
      std::move(navigation_initiator));
}

void FrameLoader::CommitNavigation(
    std::unique_ptr<WebNavigationParams> navigation_params,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  DCHECK(frame_->GetDocument());
  DCHECK(Client()->HasWebView());

  if (!frame_->IsNavigationAllowed() ||
      frame_->GetDocument()->PageDismissalEventBeingDispatched() !=
          Document::kNoDismissal) {
    // Any of the checks above should not be necessary.
    // Unfortunately, in the case of sync IPCs like print() there might be
    // reentrancy and, for example, frame detach happening.
    // See fast/loader/detach-while-printing.html for a repro.
    // TODO(https://crbug.com/862088): we should probably ignore print()
    // call in this case instead.
    return;
  }

  // TODO(dgozman): figure out the better place for this check
  // to cancel lazy load both on start and commit. Perhaps
  // CancelProvisionalLoaderForNewNavigation() is a good one.
  if (HTMLFrameOwnerElement* element = frame_->DeprecatedLocalOwner())
    element->CancelPendingLazyLoad();

  navigation_params->frame_load_type = DetermineFrameLoadType(
      navigation_params->url, navigation_params->http_method,
      nullptr /* origin_document */, navigation_params->unreachable_url,
      navigation_params->frame_load_type);

  // Note: we might actually classify this navigation as same document
  // right here in the following circumstances:
  // - the loader has already committed a navigation and notified the browser
  //   process which did not receive a message about that just yet;
  // - meanwhile, the browser process sent us a command to commit this new
  //   "cross-document" navigation, while it's actually same-document
  //   with regards to the last commit.
  // In this rare case, we intentionally proceed as cross-document.

  RecordLatestRequiredCSP();

  if (!CancelProvisionalLoaderForNewNavigation(
          false /* cancel_scheduled_navigations */,
          DocumentLoader::WillLoadUrlAsEmpty(navigation_params->url),
          false /* is_form_submission */)) {
    return;
  }

  // TODO(dgozman): navigation type should probably be passed by the caller.
  // It seems incorrect to pass |false| for |have_event| and then use
  // determined navigation type to update resource request.
  WebNavigationType navigation_type = DetermineNavigationType(
      navigation_params->frame_load_type,
      !navigation_params->http_body.IsNull(), false /* have_event */);

  HistoryItem* history_item = nullptr;
  if (IsBackForwardLoadType(navigation_params->frame_load_type)) {
    history_item = navigation_params->history_item;
    DCHECK(history_item);
  }

  // TODO(dgozman): get rid of provisional document loader and most of the code
  // below. We should probably call DocumentLoader::CommitNavigation directly.
  provisional_document_loader_ = Client()->CreateDocumentLoader(
      frame_, navigation_type, std::move(navigation_params),
      std::move(extra_data));
  if (history_item)
    provisional_document_loader_->SetItemForHistoryNavigation(history_item);
  if (!provisional_document_loader_->PrepareForLoad()) {
    DetachDocumentLoader(provisional_document_loader_);
    return;
  }

  frame_->GetFrameScheduler()->DidStartProvisionalLoad(frame_->IsMainFrame());
  Client()->DispatchDidStartProvisionalLoad(provisional_document_loader_);
  probe::DidStartProvisionalLoad(frame_);
  virtual_time_pauser_.PauseVirtualTime();
  provisional_document_loader_->StartLoading();
  TakeObjectSnapshot();
}

mojom::CommitResult FrameLoader::CommitSameDocumentNavigation(
    const KURL& url,
    WebFrameLoadType frame_load_type,
    HistoryItem* history_item,
    ClientRedirectPolicy client_redirect_policy,
    Document* origin_document,
    bool has_event,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  DCHECK(!IsReloadLoadType(frame_load_type));
  DCHECK(frame_->GetDocument());

  bool history_navigation = IsBackForwardLoadType(frame_load_type);

  if (!frame_->IsNavigationAllowed())
    return mojom::CommitResult::Aborted;

  if (!history_navigation) {
    // In the case of non-history navigations, check that this is a
    // same-document navigation. If not, the navigation should restart as a
    // cross-document navigation.
    if (!url.HasFragmentIdentifier() ||
        !EqualIgnoringFragmentIdentifier(frame_->GetDocument()->Url(), url) ||
        frame_->GetDocument()->IsFrameSet()) {
      return mojom::CommitResult::RestartCrossDocument;
    }
  }

  DCHECK(history_item || !history_navigation);
  scoped_refptr<SerializedScriptValue> state_object =
      history_navigation ? history_item->StateObject() : nullptr;

  if (!history_navigation) {
    document_loader_->SetNavigationType(
        DetermineNavigationType(frame_load_type, false, has_event));
    bool should_treat_url_as_same_as_current =
        document_loader_->GetHistoryItem() &&
        url == document_loader_->GetHistoryItem()->Url();
    if (should_treat_url_as_same_as_current)
      frame_load_type = WebFrameLoadType::kReplaceCurrentItem;
  }

  // Perform the same-document navigation.
  LoadInSameDocument(url, state_object, frame_load_type, history_item,
                     client_redirect_policy, origin_document,
                     std::move(extra_data));
  return mojom::CommitResult::Ok;
}

bool FrameLoader::CreatePlaceholderDocumentLoader(
    const WebNavigationInfo& info,
    std::unique_ptr<WebDocumentLoader::ExtraData> extra_data) {
  if (!CancelProvisionalLoaderForNewNavigation(
          true /* cancel_scheduled_navigations */,
          false /* is_starting_blank_navigation */, !info.form.IsNull())) {
    return false;
  }

  auto navigation_params = std::make_unique<WebNavigationParams>();
  navigation_params->url = info.url_request.Url();
  navigation_params->frame_load_type = info.frame_load_type;
  navigation_params->is_client_redirect = info.is_client_redirect;
  navigation_params->navigation_timings.input_start = info.input_start;
  provisional_document_loader_ = Client()->CreateDocumentLoader(
      frame_, info.navigation_type, std::move(navigation_params),
      std::move(extra_data));
  frame_->GetFrameScheduler()->DidStartProvisionalLoad(frame_->IsMainFrame());
  probe::DidStartProvisionalLoad(frame_);
  virtual_time_pauser_.PauseVirtualTime();
  TakeObjectSnapshot();
  return true;
}

void FrameLoader::StopAllLoaders() {
  if (!frame_->IsNavigationAllowed() ||
      frame_->GetDocument()->PageDismissalEventBeingDispatched() !=
          Document::kNoDismissal) {
    return;
  }

  // This method could be called from within this method, e.g. through plugin
  // detach. Avoid infinite recursion by disabling navigations.
  FrameNavigationDisabler navigation_disabler(*frame_);

  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      child_local_frame->Loader().StopAllLoaders();
  }

  frame_->GetDocument()->CancelParsing();
  if (document_loader_)
    document_loader_->StopLoading();
  DetachDocumentLoader(provisional_document_loader_);
  frame_->GetNavigationScheduler().Cancel();
  DidFinishNavigation();

  TakeObjectSnapshot();
}

void FrameLoader::DidAccessInitialDocument() {
  if (frame_->IsMainFrame()) {
    // Forbid script execution to prevent re-entering V8, since this is called
    // from a binding security check.
    ScriptForbiddenScope forbid_scripts;
    if (Client())
      Client()->DidAccessInitialDocument();
  }
}

bool FrameLoader::PrepareForCommit() {
  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  DocumentLoader* pdl = provisional_document_loader_;

  if (frame_->GetDocument()) {
    unsigned node_count = 0;
    for (Frame* frame = frame_; frame; frame = frame->Tree().TraverseNext()) {
      if (auto* local_frame = DynamicTo<LocalFrame>(frame))
        node_count += local_frame->GetDocument()->NodeCount();
    }
    unsigned total_node_count =
        InstanceCounters::CounterValue(InstanceCounters::kNodeCounter);
    float ratio = static_cast<float>(node_count) / total_node_count;
    ThreadState::Current()->SchedulePageNavigationGCIfNeeded(ratio);
  }

  // Don't allow this frame to navigate anymore. This line is needed for
  // navigation triggered from children's unload handlers. Blocking navigations
  // triggered from this frame's unload handler is already covered in
  // DispatchUnloadEvent().
  FrameNavigationDisabler navigation_disabler(*frame_);
  // Don't allow any new child frames to load in this frame: attaching a new
  // child frame during or after detaching children results in an attached frame
  // on a detached DOM tree, which is bad.
  SubframeLoadingDisabler disabler(frame_->GetDocument());
  // https://html.spec.whatwg.org/C/browsing-the-web.html#unload-a-document
  // The ignore-opens-during-unload counter of a Document must be incremented
  // both when unloading itself and when unloading its descendants.
  IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
      frame_->GetDocument());
  if (document_loader_)
    DispatchUnloadEvent();
  frame_->DetachChildren();
  // The previous calls to dispatchUnloadEvent() and detachChildren() can
  // execute arbitrary script via things like unload events. If the executed
  // script causes the current frame to be detached, we need to abandon the
  // current load.
  if (!frame_->Client())
    return false;
  // FrameNavigationDisabler should prevent another load from starting.
  DCHECK_EQ(provisional_document_loader_, pdl);
  // detachFromFrame() will abort XHRs that haven't completed, which can trigger
  // event listeners for 'abort'. These event listeners might call
  // window.stop(), which will in turn detach the provisional document loader.
  // At this point, the provisional document loader should not detach, because
  // then the FrameLoader would not have any attached DocumentLoaders. This is
  // guaranteed by FrameNavigationDisabler above.
  if (document_loader_)
    DetachDocumentLoader(document_loader_, true);
  // 'abort' listeners can also detach the frame.
  if (!frame_->Client())
    return false;
  // FrameNavigationDisabler should prevent another load from starting.
  DCHECK_EQ(provisional_document_loader_, pdl);

  // No more events will be dispatched so detach the Document.
  // TODO(yoav): Should we also be nullifying domWindow's document (or
  // domWindow) since the doc is now detached?
  if (frame_->GetDocument())
    frame_->GetDocument()->Shutdown();
  document_loader_ = provisional_document_loader_.Release();
  if (document_loader_)
    document_loader_->MarkAsCommitted();

  TakeObjectSnapshot();

  return true;
}

void FrameLoader::CommitProvisionalLoad() {
  DCHECK(Client()->HasWebView());

  // Check if the destination page is allowed to access the previous page's
  // timing information.
  if (frame_->GetDocument()) {
    scoped_refptr<const SecurityOrigin> security_origin =
        SecurityOrigin::Create(provisional_document_loader_->Url());
    provisional_document_loader_->GetTiming()
        .SetHasSameOriginAsPreviousDocument(
            security_origin->CanRequest(frame_->GetDocument()->Url()));
  }
  virtual_time_pauser_.UnpauseVirtualTime();

  if (!PrepareForCommit())
    return;

  Client()->TransitionToCommittedForNewPage();

  // If this is an about:blank navigation committing asynchronously, don't
  // cancel scheduled navigations, so that the scheduled navigation still goes
  // through. This handles the case where a navigation is scheduled between the
  // about:blank navigation starting and finishing, where previously it would
  // have happened after about:blank completed.
  // TODO(japhet): This is an atrocious hack. Get rid of NavigationScheduler
  // so it isn't needed.
  if (!state_machine_.CommittedFirstRealDocumentLoad() ||
      !DocumentLoader::WillLoadUrlAsEmpty(document_loader_->Url())) {
    frame_->GetNavigationScheduler().Cancel();
  }
}

void FrameLoader::RestoreScrollPositionAndViewState() {
  if (!frame_->GetPage() || !GetDocumentLoader() ||
      !GetDocumentLoader()->GetHistoryItem() ||
      !GetDocumentLoader()->GetHistoryItem()->GetViewState() ||
      in_restore_scroll_) {
    return;
  }
  base::AutoReset<bool> in_restore_scroll(&in_restore_scroll_, true);
  RestoreScrollPositionAndViewState(
      GetDocumentLoader()->LoadType(), false /* is_same_document */,
      *GetDocumentLoader()->GetHistoryItem()->GetViewState(),
      GetDocumentLoader()->GetHistoryItem()->ScrollRestorationType());
}

void FrameLoader::RestoreScrollPositionAndViewState(
    WebFrameLoadType load_type,
    bool is_same_document,
    const HistoryItem::ViewState& view_state,
    HistoryScrollRestorationType scroll_restoration_type) {
  LocalFrameView* view = frame_->View();
  if (!view || !view->LayoutViewport() ||
      !state_machine_.CommittedFirstRealDocumentLoad() ||
      !frame_->IsAttached()) {
    return;
  }
  if (!NeedsHistoryItemRestore(load_type))
    return;

  bool should_restore_scroll =
      scroll_restoration_type != kScrollRestorationManual;
  bool should_restore_scale = view_state.page_scale_factor_;

  // This tries to balance:
  // 1. restoring as soon as possible.
  // 2. not overriding user scroll (TODO(majidvp): also respect user scale).
  // 3. detecting clamping to avoid repeatedly popping the scroll position down
  //    as the page height increases.
  // 4. forcing a layout if necessary to avoid clamping.
  // 5. ignoring clamp detection if scroll state is not being restored, if load
  //    is complete, or if the navigation is same-document (as the new page may
  //    be smaller than the previous page).
  bool can_restore_without_clamping =
      view->LayoutViewport()->ClampScrollOffset(view_state.scroll_offset_) ==
      view_state.scroll_offset_;

  bool should_force_clamping = !frame_->IsLoading() || is_same_document;
  // Here |can_restore_without_clamping| is false, but layout might be necessary
  // to ensure correct content size.
  if (!can_restore_without_clamping && should_force_clamping)
    frame_->GetDocument()->UpdateStyleAndLayout();

  bool can_restore_without_annoying_user =
      !GetDocumentLoader()->GetInitialScrollState().was_scrolled_by_user &&
      (can_restore_without_clamping || should_force_clamping ||
       !should_restore_scroll);
  if (!can_restore_without_annoying_user)
    return;

  if (should_restore_scroll) {
    // TODO(pnoland): attempt to restore the anchor in more places than this.
    // Anchor-based restore should allow for earlier restoration.
    bool did_restore =
        RuntimeEnabledFeatures::ScrollAnchorSerializationEnabled() &&
        view->LayoutViewport()->RestoreScrollAnchor(
            {view_state.scroll_anchor_data_.selector_,
             LayoutPoint(view_state.scroll_anchor_data_.offset_.x,
                         view_state.scroll_anchor_data_.offset_.y),
             view_state.scroll_anchor_data_.simhash_});
    if (!did_restore) {
      view->LayoutViewport()->SetScrollOffset(view_state.scroll_offset_,
                                              kProgrammaticScroll);
    }
  }

  // For main frame restore scale and visual viewport position
  if (frame_->IsMainFrame()) {
    ScrollOffset visual_viewport_offset(
        view_state.visual_viewport_scroll_offset_);

    // If the visual viewport's offset is (-1, -1) it means the history item
    // is an old version of HistoryItem so distribute the scroll between
    // the main frame and the visual viewport as best as we can.
    if (visual_viewport_offset.Width() == -1 &&
        visual_viewport_offset.Height() == -1) {
      visual_viewport_offset =
          view_state.scroll_offset_ - view->LayoutViewport()->GetScrollOffset();
    }

    VisualViewport& visual_viewport = frame_->GetPage()->GetVisualViewport();
    if (should_restore_scale && should_restore_scroll) {
      visual_viewport.SetScaleAndLocation(
          view_state.page_scale_factor_, visual_viewport.IsPinchGestureActive(),
          FloatPoint(visual_viewport_offset));
    } else if (should_restore_scale) {
      visual_viewport.SetScale(view_state.page_scale_factor_);
    } else if (should_restore_scroll) {
      visual_viewport.SetLocation(FloatPoint(visual_viewport_offset));
    }

    if (ScrollingCoordinator* scrolling_coordinator =
            frame_->GetPage()->GetScrollingCoordinator())
      scrolling_coordinator->FrameViewRootLayerDidChange(view);
  }

  GetDocumentLoader()->GetInitialScrollState().did_restore_from_history = true;
}

String FrameLoader::UserAgent() const {
  String user_agent = Client()->UserAgent();
  probe::ApplyUserAgentOverride(probe::ToCoreProbeSink(frame_->GetDocument()),
                                &user_agent);
  return user_agent;
}

blink::UserAgentMetadata FrameLoader::UserAgentMetadata() const {
  // TODO(mkwst): Support overrides.
  return Client()->UserAgentMetadata();
}

void FrameLoader::Detach() {
  frame_->GetDocument()->CancelParsing();
  DetachDocumentLoader(document_loader_);
  if (provisional_document_loader_) {
    // Suppress client notification about failed provisional
    // load - it does not bring any value when the frame is
    // being detached anyway.
    provisional_document_loader_->SetSentDidFinishLoad();
    DetachDocumentLoader(provisional_document_loader_);
  }
  frame_->GetNavigationScheduler().Cancel();
  DidFinishNavigation();

  if (progress_tracker_) {
    progress_tracker_->Dispose();
    progress_tracker_.Clear();
  }

  TRACE_EVENT_OBJECT_DELETED_WITH_ID("loading", "FrameLoader", this);
  detached_ = true;
  virtual_time_pauser_.UnpauseVirtualTime();
}

void FrameLoader::DetachProvisionalDocumentLoader(DocumentLoader* loader) {
  DCHECK_EQ(loader, provisional_document_loader_);
  DetachDocumentLoader(provisional_document_loader_);
  DidFinishNavigation();
}

bool FrameLoader::ShouldPerformFragmentNavigation(bool is_form_submission,
                                                  const String& http_method,
                                                  WebFrameLoadType load_type,
                                                  const KURL& url) {
  // We don't do this if we are submitting a form with method other than "GET",
  // explicitly reloading, currently displaying a frameset, or if the URL does
  // not have a fragment.
  return DeprecatedEqualIgnoringCase(http_method, http_names::kGET) &&
         !IsReloadLoadType(load_type) &&
         load_type != WebFrameLoadType::kBackForward &&
         url.HasFragmentIdentifier() &&
         // For provisional LocalFrame, there is no real document loaded and
         // the initial empty document should not be considered, so there is
         // no way to get a same-document load in this case.
         !frame_->IsProvisional() &&
         EqualIgnoringFragmentIdentifier(frame_->GetDocument()->Url(), url)
         // We don't want to just scroll if a link from within a frameset is
         // trying to reload the frameset into _top.
         && !frame_->GetDocument()->IsFrameSet();
}

void FrameLoader::ProcessFragment(const KURL& url,
                                  WebFrameLoadType frame_load_type,
                                  LoadStartType load_start_type) {
  LocalFrameView* view = frame_->View();
  if (!view)
    return;

  // Leaking scroll position to a cross-origin ancestor would permit the
  // so-called "framesniffing" attack.
  Frame* boundary_frame =
      url.HasFragmentIdentifier()
          ? frame_->FindUnsafeParentScrollPropagationBoundary()
          : nullptr;

  // FIXME: Handle RemoteFrames
  if (auto* boundary_local_frame = DynamicTo<LocalFrame>(boundary_frame))
    boundary_local_frame->View()->SetSafeToPropagateScrollToParent(false);

  // If scroll position is restored from history fragment or scroll
  // restoration type is manual, then we should not override it unless this
  // is a same document reload.
  bool should_scroll_to_fragment =
      (load_start_type == kNavigationWithinSameDocument &&
       !IsBackForwardLoadType(frame_load_type)) ||
      (!GetDocumentLoader()->GetInitialScrollState().did_restore_from_history &&
       !(GetDocumentLoader()->GetHistoryItem() &&
         GetDocumentLoader()->GetHistoryItem()->ScrollRestorationType() ==
             kScrollRestorationManual));

  view->ProcessUrlFragment(url, should_scroll_to_fragment);

  if (auto* boundary_local_frame = DynamicTo<LocalFrame>(boundary_frame))
    boundary_local_frame->View()->SetSafeToPropagateScrollToParent(true);
}

bool FrameLoader::ShouldClose(bool is_reload) {
  Page* page = frame_->GetPage();
  if (!page || !page->GetChromeClient().CanOpenBeforeUnloadConfirmPanel())
    return true;

  HeapVector<Member<LocalFrame>> descendant_frames;
  for (Frame* child = frame_->Tree().FirstChild(); child;
       child = child->Tree().TraverseNext(frame_)) {
    // FIXME: There is not yet any way to dispatch events to out-of-process
    // frames.
    if (auto* child_local_frame = DynamicTo<LocalFrame>(child))
      descendant_frames.push_back(child_local_frame);
  }

  {
    NavigationDisablerForBeforeUnload navigation_disabler;
    bool did_allow_navigation = false;

    // https://html.spec.whatwg.org/C/browsing-the-web.html#prompt-to-unload-a-document

    // First deal with this frame.
    IgnoreOpensDuringUnloadCountIncrementer ignore_opens_during_unload(
        frame_->GetDocument());
    if (!frame_->GetDocument()->DispatchBeforeUnloadEvent(
            &page->GetChromeClient(), is_reload, did_allow_navigation))
      return false;

    // Then deal with descendent frames.
    for (Member<LocalFrame>& descendant_frame : descendant_frames) {
      if (!descendant_frame->Tree().IsDescendantOf(frame_))
        continue;

      // There is some confusion in the spec around what counters should be
      // incremented for a descendant browsing context:
      // https://github.com/whatwg/html/issues/3899
      //
      // Here for implementation ease, we use the current spec behavior, which
      // is to increment only the counter of the Document on which this is
      // called, and that of the Document we are firing the beforeunload event
      // on -- not any intermediate Documents that may be the parent of the
      // frame being unloaded but is not root Document.
      IgnoreOpensDuringUnloadCountIncrementer
          ignore_opens_during_unload_descendant(
              descendant_frame->GetDocument());
      if (!descendant_frame->GetDocument()->DispatchBeforeUnloadEvent(
              &page->GetChromeClient(), is_reload, did_allow_navigation))
        return false;
    }
  }

  return true;
}

void FrameLoader::ClientDroppedNavigation() {
  if (!provisional_document_loader_ || provisional_document_loader_->DidStart())
    return;

  DetachProvisionalDocumentLoader(provisional_document_loader_);
  // Forcibly instantiate WindowProxy for initial frame document.
  // This is only required when frame navigation is aborted, e.g. due to
  // mixed content.
  // TODO(lushnikov): this should be done in Init for initial empty doc, but
  // that breaks extensions abusing SetForceMainWorldInitialization setting
  // and relying on the number of created window proxies.
  Settings* settings = frame_->GetSettings();
  if (settings && settings->GetForceMainWorldInitialization()) {
    // Forcibly instantiate WindowProxy.
    frame_->GetScriptController().WindowProxy(DOMWrapperWorld::MainWorld());
  }
}

void FrameLoader::MarkAsLoading() {
  // This should only be called for initial history navigation in child frame.
  DCHECK(!provisional_document_loader_);
  DCHECK(frame_->GetDocument()->IsLoadCompleted());
  DCHECK(frame_->GetDocument()->HasFinishedParsing());
  progress_tracker_->ProgressStarted();
}

bool FrameLoader::ShouldReuseDefaultView(const KURL& url,
                                         const ContentSecurityPolicy* csp) {
  // Secure transitions can only happen when navigating from the initial empty
  // document.
  if (!state_machine_.IsDisplayingInitialEmptyDocument())
    return false;

  // The Window object should only be re-used if it is same-origin.
  // Since sandboxing turns the origin into an opaque origin it needs to also
  // be considered when deciding whether to reuse it.
  // Spec:
  // https://html.spec.whatwg.org/C/#initialise-the-document-object
  if (csp && (csp->GetSandboxMask() & WebSandboxFlags::kOrigin) !=
                 WebSandboxFlags::kNone) {
    return false;
  }

  scoped_refptr<const SecurityOrigin> origin = SecurityOrigin::Create(url);
  return frame_->GetDocument()->GetSecurityOrigin()->CanAccess(origin.get());
}

bool FrameLoader::CancelProvisionalLoaderForNewNavigation(
    bool cancel_scheduled_navigations,
    bool is_starting_blank_navigation,
    bool is_form_submission) {
  bool had_placeholder_client_document_loader =
      provisional_document_loader_ && !provisional_document_loader_->DidStart();

  // For placeholder DocumentLoaders, don't send failure callbacks
  // for a placeholder simply being replaced with a new DocumentLoader.
  if (had_placeholder_client_document_loader)
    provisional_document_loader_->SetSentDidFinishLoad();

  // This seems to correspond to step 9 of the specification:
  // "9. Abort the active document of browsingContext."
  // https://html.spec.whatwg.org/C/#navigate
  frame_->GetDocument()->Abort(is_form_submission);
  // document.onreadystatechange can fire in Abort(), which can:
  // 1) Detach this frame.
  // 2) Stop the provisional DocumentLoader (i.e window.stop()).
  if (!frame_->GetPage())
    return false;

  DetachDocumentLoader(provisional_document_loader_);
  // Detaching the provisional DocumentLoader above may leave the frame without
  // any loading DocumentLoader. It can causes the 'load' event to fire, which
  // can be used to detach this frame.
  if (!frame_->GetPage())
    return false;

  progress_tracker_->ProgressStarted();

  // If this is an about:blank navigation committing asynchronously, don't
  // cancel scheduled navigations, so that the scheduled navigation still goes
  // through. This handles the case where a navigation is scheduled between the
  // about:blank navigation starting and finishing, where previously it would
  // have happened after about:blank completed.
  // TODO(japhet): This is an atrocious hack. Get rid of NavigationScheduler
  // so it isn't needed.
  bool skip_cancel_for_about_blank =
      state_machine_.CommittedFirstRealDocumentLoad() &&
      is_starting_blank_navigation;
  // We need to ensure that script initiated navigations are honored.
  if (!skip_cancel_for_about_blank &&
      (!had_placeholder_client_document_loader ||
       cancel_scheduled_navigations)) {
    frame_->GetNavigationScheduler().Cancel();
  }

  return true;
}

void FrameLoader::DispatchDocumentElementAvailable() {
  ScriptForbiddenScope forbid_scripts;
  Client()->DocumentElementAvailable();
}

void FrameLoader::RunScriptsAtDocumentElementAvailable() {
  Client()->RunScriptsAtDocumentElementAvailable();
  // The frame might be detached at this point.
}

void FrameLoader::DispatchDidClearDocumentOfWindowObject() {
  DCHECK(frame_->GetDocument());
  if (state_machine_.CreatingInitialEmptyDocument())
    return;

  Settings* settings = frame_->GetSettings();
  if (settings && settings->GetForceMainWorldInitialization()) {
    // Forcibly instantiate WindowProxy, even if script is disabled.
    frame_->GetScriptController().WindowProxy(DOMWrapperWorld::MainWorld());
  }
  probe::DidClearDocumentOfWindowObject(frame_);
  if (!frame_->GetDocument()->CanExecuteScripts(kNotAboutToExecuteScript))
    return;

  if (dispatching_did_clear_window_object_in_main_world_)
    return;
  base::AutoReset<bool> in_did_clear_window_object(
      &dispatching_did_clear_window_object_in_main_world_, true);
  // We just cleared the document, not the entire window object, but for the
  // embedder that's close enough.
  Client()->DispatchDidClearWindowObjectInMainWorld();
}

void FrameLoader::DispatchDidClearWindowObjectInMainWorld() {
  DCHECK(frame_->GetDocument());
  if (!frame_->GetDocument()->CanExecuteScripts(kNotAboutToExecuteScript))
    return;

  if (dispatching_did_clear_window_object_in_main_world_)
    return;
  base::AutoReset<bool> in_did_clear_window_object(
      &dispatching_did_clear_window_object_in_main_world_, true);
  Client()->DispatchDidClearWindowObjectInMainWorld();
}

SandboxFlags FrameLoader::EffectiveSandboxFlags() const {
  SandboxFlags flags = forced_sandbox_flags_;
  if (FrameOwner* frame_owner = frame_->Owner())
    flags |= frame_owner->GetFramePolicy().sandbox_flags;
  // Frames need to inherit the sandbox flags of their parent frame.
  if (Frame* parent_frame = frame_->Tree().Parent())
    flags |= parent_frame->GetSecurityContext()->GetSandboxFlags();
  return flags;
}

void FrameLoader::ModifyRequestForCSP(
    ResourceRequest& resource_request,
    Document* origin_document,
    network::mojom::RequestContextFrameType frame_type) const {
  if (!RequiredCSP().IsEmpty()) {
    DCHECK(
        ContentSecurityPolicy::IsValidCSPAttr(RequiredCSP().GetString(), ""));
    resource_request.SetHttpHeaderField(http_names::kSecRequiredCSP,
                                        RequiredCSP());
  }

  // Tack an 'Upgrade-Insecure-Requests' header to outgoing navigational
  // requests, as described in
  // https://w3c.github.io/webappsec-upgrade-insecure-requests/#feature-detect
  if (frame_type != network::mojom::RequestContextFrameType::kNone) {
    // Early return if the request has already been upgraded.
    if (!resource_request.HttpHeaderField(http_names::kUpgradeInsecureRequests)
             .IsNull()) {
      return;
    }

    resource_request.SetHttpHeaderField(http_names::kUpgradeInsecureRequests,
                                        "1");
  }

  UpgradeInsecureRequest(resource_request, origin_document, frame_type);
}

// static
void FrameLoader::UpgradeInsecureRequest(
    ResourceRequest& resource_request,
    ExecutionContext* origin_context,
    network::mojom::RequestContextFrameType frame_type) {
  // We always upgrade requests that meet any of the following criteria:
  //  1. Are for subresources.
  //  2. Are for nested frames.
  //  3. Are form submissions.
  //  4. Whose hosts are contained in the origin_context's upgrade insecure
  //     navigations set.

  // This happens for:
  // * Browser initiated main document loading. No upgrade required.
  // * Navigation initiated by a frame in another process. URL should have
  //   already been upgraded in the initiator's process.
  if (!origin_context)
    return;

  if (!(origin_context->GetSecurityContext().GetInsecureRequestPolicy() &
        kUpgradeInsecureRequests)) {
    mojom::RequestContextType context = resource_request.GetRequestContext();
    // TODO(carlosil): Handle strict_mixed_content_checking_for_plugin
    // correctly.
    if (context != mojom::RequestContextType::UNSPECIFIED &&
        resource_request.Url().ProtocolIs("http") &&
        !origin_context->GetSecurityContext().GetMixedAutoUpgradeOptOut() &&
        MixedContentChecker::ShouldAutoupgrade(
            origin_context->GetHttpsState(),
            WebMixedContent::ContextTypeFromRequestContext(context, false))) {
      if (origin_context->IsDocument()) {
        Document* document = static_cast<Document*>(origin_context);
        document->AddConsoleMessage(
            MixedContentChecker::CreateConsoleMessageAboutFetchAutoupgrade(
                origin_context->Url(), resource_request.Url()));
        resource_request.SetUkmSourceId(document->UkmSourceID());
      }
      resource_request.SetIsAutomaticUpgrade(true);
    } else {
      return;
    }
  }

  // Nested frames are always upgraded on the browser process.
  if (frame_type == network::mojom::RequestContextFrameType::kNested)
    return;

  // We set the UpgradeIfInsecure flag even if the current request wasn't
  // upgraded (due to already being HTTPS), since we still need to upgrade
  // redirects if they are not to HTTPS URLs.
  resource_request.SetUpgradeIfInsecure(true);

  KURL url = resource_request.Url();

  if (!url.ProtocolIs("http") ||
      SecurityOrigin::Create(url)->IsPotentiallyTrustworthy()) {
    return;
  }

  if (frame_type == network::mojom::RequestContextFrameType::kNone ||
      resource_request.GetRequestContext() == mojom::RequestContextType::FORM ||
      (!url.Host().IsNull() && origin_context->GetSecurityContext()
                                   .InsecureNavigationsToUpgrade()
                                   ->Contains(url.Host().Impl()->GetHash()))) {
    UseCounter::Count(origin_context,
                      WebFeature::kUpgradeInsecureRequestsUpgradedRequest);
    url.SetProtocol("https");
    if (url.Port() == 80)
      url.SetPort(443);
    resource_request.SetUrl(url);
  }
}

void FrameLoader::RecordLatestRequiredCSP() {
  required_csp_ =
      frame_->Owner() ? frame_->Owner()->RequiredCsp() : g_null_atom;
}

std::unique_ptr<TracedValue> FrameLoader::ToTracedValue() const {
  auto traced_value = std::make_unique<TracedValue>();
  traced_value->BeginDictionary("frame");
  traced_value->SetString("id_ref", IdentifiersFactory::FrameId(frame_.Get()));
  traced_value->EndDictionary();
  traced_value->SetBoolean("isLoadingMainFrame", frame_->IsMainFrame());
  traced_value->SetString("stateMachine", state_machine_.ToString());
  traced_value->SetString("provisionalDocumentLoaderURL",
                          provisional_document_loader_
                              ? provisional_document_loader_->Url().GetString()
                              : String());
  traced_value->SetString(
      "documentLoaderURL",
      document_loader_ ? document_loader_->Url().GetString() : String());
  return traced_value;
}

inline void FrameLoader::TakeObjectSnapshot() const {
  if (detached_) {
    // We already logged TRACE_EVENT_OBJECT_DELETED_WITH_ID in detach().
    return;
  }
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID("loading", "FrameLoader", this,
                                      ToTracedValue());
}

STATIC_ASSERT_ENUM(kWebHistoryScrollRestorationManual,
                   kScrollRestorationManual);
STATIC_ASSERT_ENUM(kWebHistoryScrollRestorationAuto, kScrollRestorationAuto);

}  // namespace blink
