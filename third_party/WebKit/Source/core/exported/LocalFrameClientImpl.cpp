/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/exported/LocalFrameClientImpl.h"

#include <memory>

#include "bindings/core/v8/ScriptController.h"
#include "core/CoreInitializer.h"
#include "core/dom/Document.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/events/MessageEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/UIEventWithKeyState.h"
#include "core/exported/SharedWorkerRepositoryClientImpl.h"
#include "core/exported/WebDevToolsAgentImpl.h"
#include "core/exported/WebDevToolsFrontendImpl.h"
#include "core/exported/WebDocumentLoaderImpl.h"
#include "core/exported/WebPluginContainerImpl.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/html_names.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/DevToolsEmulator.h"
#include "core/layout/HitTestResult.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/page/Page.h"
#include "platform/Histogram.h"
#include "platform/WebFrameScheduler.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/network/HTTPParsers.h"
#include "platform/plugins/PluginData.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StringExtras.h"
#include "platform/wtf/text/CString.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"
#include "public/platform/WebApplicationCacheHost.h"
#include "public/platform/WebMediaPlayerSource.h"
#include "public/platform/WebRTCPeerConnectionHandler.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProvider.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerProviderClient.h"
#include "public/web/WebAutofillClient.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebNode.h"
#include "public/web/WebPlugin.h"
#include "public/web/WebPluginParams.h"
#include "public/web/WebViewClient.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Print up to |kMaxCertificateWarningMessages| console messages per frame
// about certificates that will be distrusted in future.
const uint32_t kMaxCertificateWarningMessages = 10;

// Convenience helper for frame tree helpers in FrameClient to reduce the amount
// of null-checking boilerplate code. Since the frame tree is maintained in the
// web/ layer, the frame tree helpers often have to deal with null WebFrames:
// for example, a frame with no parent will return null for WebFrame::parent().
// TODO(dcheng): Remove duplication between LocalFrameClientImpl and
// RemoteFrameClientImpl somehow...
Frame* ToCoreFrame(WebFrame* frame) {
  return frame ? WebFrame::ToCoreFrame(*frame) : nullptr;
}

// Return the parent of |frame| as a LocalFrame, nullptr when there is no
// parent or when the parent is a remote frame.
LocalFrame* GetLocalParentFrame(WebLocalFrameImpl* frame) {
  WebFrame* parent = frame->Parent();
  if (!parent || !parent->IsWebLocalFrame())
    return nullptr;

  return ToWebLocalFrameImpl(parent)->GetFrame();
}

// Returns whether the |local_frame| has been loaded using an MHTMLArchive. When
// it is the case, each subframe must use it for loading.
bool IsLoadedAsMHTMLArchive(LocalFrame* local_frame) {
  return local_frame && local_frame->GetDocument()->Fetcher()->Archive();
}

// Returns whether the |local_frame| is in a middle of a back/forward
// navigation.
bool IsBackForwardNavigationInProgress(LocalFrame* local_frame) {
  return local_frame &&
         IsBackForwardLoadType(
             local_frame->Loader().GetDocumentLoader()->LoadType()) &&
         !local_frame->GetDocument()->LoadEventFinished();
}

}  // namespace

LocalFrameClientImpl::LocalFrameClientImpl(WebLocalFrameImpl* frame)
    : web_frame_(frame), num_certificate_warning_messages_(0) {}

LocalFrameClientImpl* LocalFrameClientImpl::Create(WebLocalFrameImpl* frame) {
  return new LocalFrameClientImpl(frame);
}

LocalFrameClientImpl::~LocalFrameClientImpl() {}

void LocalFrameClientImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(web_frame_);
  LocalFrameClient::Trace(visitor);
}

WebLocalFrameImpl* LocalFrameClientImpl::GetWebFrame() const {
  return web_frame_.Get();
}

void LocalFrameClientImpl::DidCreateNewDocument() {
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateNewDocument();
}

void LocalFrameClientImpl::DispatchDidClearWindowObjectInMainWorld() {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidClearWindowObject();
    Document* document = web_frame_->GetFrame()->GetDocument();
    if (document) {
      const Settings* const settings = web_frame_->GetFrame()->GetSettings();
      CoreInitializer::GetInstance().OnClearWindowObjectInMainWorld(*document,
                                                                    *settings);
    }
  }
  // FIXME: when extensions go out of process, this whole concept stops working.
  WebDevToolsFrontendImpl* dev_tools_frontend =
      web_frame_->Top()->IsWebLocalFrame()
          ? ToWebLocalFrameImpl(web_frame_->Top())->DevToolsFrontend()
          : nullptr;
  if (dev_tools_frontend)
    dev_tools_frontend->DidClearWindowObject(web_frame_);
}

void LocalFrameClientImpl::DocumentElementAvailable() {
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateDocumentElement();
}

void LocalFrameClientImpl::RunScriptsAtDocumentElementAvailable() {
  if (web_frame_->Client())
    web_frame_->Client()->RunScriptsAtDocumentElementAvailable();
  // The callback might have deleted the frame, do not use |this|!
}

void LocalFrameClientImpl::RunScriptsAtDocumentReady(bool document_is_empty) {
  if (!document_is_empty && IsLoadedAsMHTMLArchive(web_frame_->GetFrame())) {
    // For MHTML pages, recreate the shadow DOM contents from the templates that
    // are captured from the shadow DOM trees at serialization.
    // Note that the MHTML page is loaded in sandboxing mode with script
    // execution disabled and thus only the following script will be executed.
    // Any other scripts and event handlers outside the scope of the following
    // script, including those that may be inserted in shadow DOM templates,
    // will NOT be run.
    String script = R"(
function createShadowRootWithin(node) {
  var nodes = node.querySelectorAll('template[shadowmode]');
  for (var i = 0; i < nodes.length; ++i) {
    var template = nodes[i];
    var mode = template.getAttribute('shadowmode');
    var parent = template.parentNode;
    if (!parent)
      continue;
    parent.removeChild(template);
    var shadowRoot;
    if (mode == 'v0') {
      shadowRoot = parent.createShadowRoot();
    } else if (mode == 'open' || mode == 'closed') {
      var delegatesFocus = template.hasAttribute('shadowdelegatesfocus');
      shadowRoot = parent.attachShadow({'mode': mode,
                                        'delegatesFocus': delegatesFocus});
    }
    if (!shadowRoot)
      continue;
    var clone = document.importNode(template.content, true);
    shadowRoot.appendChild(clone);
    createShadowRootWithin(shadowRoot);
  }
}
createShadowRootWithin(document.body);
)";
    web_frame_->GetFrame()->GetScriptController().ExecuteScriptInMainWorld(
        script, ScriptController::kExecuteScriptWhenScriptsDisabled);
  }

  if (web_frame_->Client()) {
    web_frame_->Client()->RunScriptsAtDocumentReady(document_is_empty);
  }
  // The callback might have deleted the frame, do not use |this|!
}

void LocalFrameClientImpl::RunScriptsAtDocumentIdle() {
  if (web_frame_->Client())
    web_frame_->Client()->RunScriptsAtDocumentIdle();
  // The callback might have deleted the frame, do not use |this|!
}

void LocalFrameClientImpl::DidCreateScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateScriptContext(context, world_id);
}

void LocalFrameClientImpl::WillReleaseScriptContext(
    v8::Local<v8::Context> context,
    int world_id) {
  if (web_frame_->Client()) {
    web_frame_->Client()->WillReleaseScriptContext(context, world_id);
  }
}

bool LocalFrameClientImpl::AllowScriptExtensions() {
  return true;
}

void LocalFrameClientImpl::DidChangeScrollOffset() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeScrollOffset();
}

void LocalFrameClientImpl::DidUpdateCurrentHistoryItem() {
  if (web_frame_->Client())
    web_frame_->Client()->DidUpdateCurrentHistoryItem();
}

bool LocalFrameClientImpl::AllowContentInitiatedDataUrlNavigations(
    const KURL& url) {
  if (RuntimeEnabledFeatures::AllowContentInitiatedDataUrlNavigationsEnabled())
    return true;
  if (web_frame_->Client())
    return web_frame_->Client()->AllowContentInitiatedDataUrlNavigations(url);
  return false;
}

bool LocalFrameClientImpl::HasWebView() const {
  return web_frame_->ViewImpl();
}

bool LocalFrameClientImpl::InShadowTree() const {
  return web_frame_->InShadowTree();
}

Frame* LocalFrameClientImpl::Opener() const {
  return ToCoreFrame(web_frame_->Opener());
}

void LocalFrameClientImpl::SetOpener(Frame* opener) {
  WebFrame* opener_frame = WebFrame::FromFrame(opener);
  if (web_frame_->Client() && web_frame_->Opener() != opener_frame)
    web_frame_->Client()->DidChangeOpener(opener_frame);
  web_frame_->SetOpener(opener_frame);
}

Frame* LocalFrameClientImpl::Parent() const {
  return ToCoreFrame(web_frame_->Parent());
}

Frame* LocalFrameClientImpl::Top() const {
  return ToCoreFrame(web_frame_->Top());
}

Frame* LocalFrameClientImpl::NextSibling() const {
  return ToCoreFrame(web_frame_->NextSibling());
}

Frame* LocalFrameClientImpl::FirstChild() const {
  return ToCoreFrame(web_frame_->FirstChild());
}

void LocalFrameClientImpl::WillBeDetached() {
  web_frame_->WillBeDetached();
}

void LocalFrameClientImpl::Detached(FrameDetachType type) {
  // Alert the client that the frame is being detached. This is the last
  // chance we have to communicate with the client.
  WebFrameClient* client = web_frame_->Client();
  if (!client)
    return;

  web_frame_->WillDetachParent();

  // Signal that no further communication with WebFrameClient should take
  // place at this point since we are no longer associated with the Page.
  web_frame_->SetClient(nullptr);

  client->FrameDetached(static_cast<WebFrameClient::DetachType>(type));

  if (type == FrameDetachType::kRemove)
    web_frame_->DetachFromParent();

  // Clear our reference to LocalFrame at the very end, in case the client
  // refers to it.
  web_frame_->SetCoreFrame(nullptr);
}

void LocalFrameClientImpl::DispatchWillSendRequest(ResourceRequest& request) {
  // Give the WebFrameClient a crack at the request.
  if (web_frame_->Client()) {
    WrappedResourceRequest webreq(request);
    web_frame_->Client()->WillSendRequest(webreq);
  }
}

void LocalFrameClientImpl::DispatchDidReceiveResponse(
    const ResourceResponse& response) {
  if (web_frame_->Client()) {
    WrappedResourceResponse webresp(response);
    web_frame_->Client()->DidReceiveResponse(webresp);
  }
}

void LocalFrameClientImpl::DispatchDidFinishDocumentLoad() {
  // TODO(dglazkov): Sadly, workers are WebFrameClients, and they can totally
  // destroy themselves when didFinishDocumentLoad is invoked, and in turn
  // destroy the fake WebLocalFrame that they create, which means that you
  // should not put any code touching `this` after the two lines below.
  if (web_frame_->Client())
    web_frame_->Client()->DidFinishDocumentLoad();
}

void LocalFrameClientImpl::DispatchDidLoadResourceFromMemoryCache(
    const ResourceRequest& request,
    const ResourceResponse& response) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidLoadResourceFromMemoryCache(
        WrappedResourceRequest(request), WrappedResourceResponse(response));
  }
}

void LocalFrameClientImpl::DispatchDidHandleOnloadEvents() {
  if (web_frame_->Client())
    web_frame_->Client()->DidHandleOnloadEvents();
}

void LocalFrameClientImpl::
    DispatchDidReceiveServerRedirectForProvisionalLoad() {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidReceiveServerRedirectForProvisionalLoad();
  }
}

void LocalFrameClientImpl::DispatchDidNavigateWithinPage(
    HistoryItem* item,
    HistoryCommitType commit_type,
    bool content_initiated) {
  bool should_create_history_entry = commit_type == kStandardCommit;
  // TODO(dglazkov): Does this need to be called for subframes?
  web_frame_->ViewImpl()->DidCommitLoad(should_create_history_entry, true);
  if (web_frame_->Client()) {
    web_frame_->Client()->DidNavigateWithinPage(
        WebHistoryItem(item), static_cast<WebHistoryCommitType>(commit_type),
        content_initiated);
  }
}

void LocalFrameClientImpl::DispatchWillCommitProvisionalLoad() {
  if (web_frame_->Client())
    web_frame_->Client()->WillCommitProvisionalLoad();
}

void LocalFrameClientImpl::DispatchDidStartProvisionalLoad(
    DocumentLoader* loader,
    ResourceRequest& request) {
  if (web_frame_->Client()) {
    WrappedResourceRequest wrapped_request(request);
    web_frame_->Client()->DidStartProvisionalLoad(
        WebDocumentLoaderImpl::FromDocumentLoader(loader), wrapped_request);
  }
  if (WebDevToolsAgentImpl* dev_tools = DevToolsAgent())
    dev_tools->DidStartProvisionalLoad(web_frame_->GetFrame());
}

void LocalFrameClientImpl::DispatchDidReceiveTitle(const String& title) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidReceiveTitle(title, kWebTextDirectionLeftToRight);
  }
}

void LocalFrameClientImpl::DispatchDidChangeIcons(IconType type) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidChangeIcon(static_cast<WebIconURL::Type>(type));
  }
}

void LocalFrameClientImpl::DispatchDidCommitLoad(
    HistoryItem* item,
    HistoryCommitType commit_type) {
  if (!web_frame_->Parent()) {
    web_frame_->ViewImpl()->DidCommitLoad(commit_type == kStandardCommit,
                                          false);
  }

  if (web_frame_->Client()) {
    web_frame_->Client()->DidCommitProvisionalLoad(
        WebHistoryItem(item), static_cast<WebHistoryCommitType>(commit_type));
  }
  if (WebDevToolsAgentImpl* dev_tools = DevToolsAgent())
    dev_tools->DidCommitLoadForLocalFrame(web_frame_->GetFrame());

  // Reset certificate warning state that prevents log spam.
  num_certificate_warning_messages_ = 0;
  certificate_warning_hosts_.clear();
}

void LocalFrameClientImpl::DispatchDidFailProvisionalLoad(
    const ResourceError& error,
    HistoryCommitType commit_type) {
  web_frame_->DidFail(error, true, commit_type);
}

void LocalFrameClientImpl::DispatchDidFailLoad(const ResourceError& error,
                                               HistoryCommitType commit_type) {
  web_frame_->DidFail(error, false, commit_type);
}

void LocalFrameClientImpl::DispatchDidFinishLoad() {
  web_frame_->DidFinish();
}

void LocalFrameClientImpl::DispatchDidChangeThemeColor() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeThemeColor();
}

static bool AllowCreatingBackgroundTabs() {
  const WebInputEvent* input_event = WebViewImpl::CurrentInputEvent();
  if (!input_event || (input_event->GetType() != WebInputEvent::kMouseUp &&
                       (input_event->GetType() != WebInputEvent::kRawKeyDown &&
                        input_event->GetType() != WebInputEvent::kKeyDown) &&
                       input_event->GetType() != WebInputEvent::kGestureTap))
    return false;

  unsigned short button_number;
  if (WebInputEvent::IsMouseEventType(input_event->GetType())) {
    const WebMouseEvent* mouse_event =
        static_cast<const WebMouseEvent*>(input_event);

    switch (mouse_event->button) {
      case WebMouseEvent::Button::kLeft:
        button_number = 0;
        break;
      case WebMouseEvent::Button::kMiddle:
        button_number = 1;
        break;
      case WebMouseEvent::Button::kRight:
        button_number = 2;
        break;
      default:
        return false;
    }
  } else {
    // The click is simulated when triggering the keypress event.
    button_number = 0;
  }
  bool ctrl = input_event->GetModifiers() & WebMouseEvent::kControlKey;
  bool shift = input_event->GetModifiers() & WebMouseEvent::kShiftKey;
  bool alt = input_event->GetModifiers() & WebMouseEvent::kAltKey;
  bool meta = input_event->GetModifiers() & WebMouseEvent::kMetaKey;

  NavigationPolicy user_policy;
  if (!NavigationPolicyFromMouseEvent(button_number, ctrl, shift, alt, meta,
                                      &user_policy))
    return false;
  return user_policy == kNavigationPolicyNewBackgroundTab;
}

NavigationPolicy LocalFrameClientImpl::DecidePolicyForNavigation(
    const ResourceRequest& request,
    Document* origin_document,
    DocumentLoader* document_loader,
    NavigationType type,
    NavigationPolicy policy,
    bool replaces_current_history_item,
    bool is_client_redirect,
    WebTriggeringEventInfo triggering_event_info,
    HTMLFormElement* form,
    ContentSecurityPolicyDisposition
        should_check_main_world_content_security_policy) {
  if (!web_frame_->Client())
    return kNavigationPolicyIgnore;

  if (policy == kNavigationPolicyNewBackgroundTab &&
      !AllowCreatingBackgroundTabs() &&
      !UIEventWithKeyState::NewTabModifierSetFromIsolatedWorld())
    policy = kNavigationPolicyNewForegroundTab;

  WebDocumentLoaderImpl* web_document_loader =
      WebDocumentLoaderImpl::FromDocumentLoader(document_loader);

  WrappedResourceRequest wrapped_resource_request(request);
  WebFrameClient::NavigationPolicyInfo navigation_info(
      wrapped_resource_request);
  navigation_info.navigation_type = static_cast<WebNavigationType>(type);
  navigation_info.default_policy = static_cast<WebNavigationPolicy>(policy);
  navigation_info.extra_data =
      web_document_loader ? web_document_loader->GetExtraData() : nullptr;
  navigation_info.replaces_current_history_item = replaces_current_history_item;
  navigation_info.is_client_redirect = is_client_redirect;
  navigation_info.triggering_event_info = triggering_event_info;
  navigation_info.should_check_main_world_content_security_policy =
      should_check_main_world_content_security_policy ==
              kCheckContentSecurityPolicy
          ? kWebContentSecurityPolicyDispositionCheck
          : kWebContentSecurityPolicyDispositionDoNotCheck;

  // Can be null.
  LocalFrame* local_parent_frame = GetLocalParentFrame(web_frame_);

  // Newly created child frames may need to be navigated to a history item
  // during a back/forward navigation. This will only happen when the parent
  // is a LocalFrame doing a back/forward navigation that has not completed.
  // (If the load has completed and the parent later adds a frame with script,
  // we do not want to use a history item for it.)
  navigation_info.is_history_navigation_in_new_child_frame =
      IsBackForwardNavigationInProgress(local_parent_frame);

  // TODO(nasko): How should this work with OOPIF?
  // The MHTMLArchive is parsed as a whole, but can be constructed from frames
  // in multiple processes. In that case, which process should parse it and how
  // should the output be spread back across multiple processes?
  navigation_info.archive_status =
      IsLoadedAsMHTMLArchive(local_parent_frame)
          ? WebFrameClient::NavigationPolicyInfo::ArchiveStatus::Present
          : WebFrameClient::NavigationPolicyInfo::ArchiveStatus::Absent;

  // Caching could be disabled for requests initiated by DevTools.
  // TODO(ananta)
  // We should extract the network cache state into a global component which
  // can be queried here and wherever necessary.
  navigation_info.is_cache_disabled =
      DevToolsAgent() ? DevToolsAgent()->CacheDisabled() : false;
  if (form)
    navigation_info.form = WebFormElement(form);

  // The frame has navigated either by itself or by the action of the
  // |origin_document| when it is defined. |source_location| represents the
  // line of code that has initiated the navigation. It is used to let web
  // developpers locate the root cause of blocked navigations.
  std::unique_ptr<SourceLocation> source_location =
      origin_document
          ? SourceLocation::Capture(origin_document)
          : SourceLocation::Capture(web_frame_->GetFrame()->GetDocument());
  if (source_location && !source_location->IsUnknown()) {
    navigation_info.source_location.url = source_location->Url();
    navigation_info.source_location.line_number = source_location->LineNumber();
    navigation_info.source_location.column_number =
        source_location->ColumnNumber();
  }

  WebNavigationPolicy web_policy =
      web_frame_->Client()->DecidePolicyForNavigation(navigation_info);
  return static_cast<NavigationPolicy>(web_policy);
}

void LocalFrameClientImpl::DispatchWillSendSubmitEvent(HTMLFormElement* form) {
  if (web_frame_->Client())
    web_frame_->Client()->WillSendSubmitEvent(WebFormElement(form));
}

void LocalFrameClientImpl::DispatchWillSubmitForm(HTMLFormElement* form) {
  if (web_frame_->Client())
    web_frame_->Client()->WillSubmitForm(WebFormElement(form));
}

void LocalFrameClientImpl::DidStartLoading(LoadStartType load_start_type) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidStartLoading(load_start_type ==
                                          kNavigationToDifferentDocument);
  }
}

void LocalFrameClientImpl::ProgressEstimateChanged(double progress_estimate) {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeLoadProgress(progress_estimate);
}

void LocalFrameClientImpl::DidStopLoading() {
  if (web_frame_->Client())
    web_frame_->Client()->DidStopLoading();
}

void LocalFrameClientImpl::DownloadURL(const ResourceRequest& request,
                                       const String& suggested_name) {
  if (!web_frame_->Client())
    return;
  DCHECK(web_frame_->GetFrame()->GetDocument());
  web_frame_->Client()->DownloadURL(WrappedResourceRequest(request),
                                    suggested_name);
}

void LocalFrameClientImpl::LoadErrorPage(int reason) {
  if (web_frame_->Client())
    web_frame_->Client()->LoadErrorPage(reason);
}

bool LocalFrameClientImpl::NavigateBackForward(int offset) const {
  WebViewImpl* webview = web_frame_->ViewImpl();
  if (!webview->Client())
    return false;

  DCHECK(offset);
  if (offset > webview->Client()->HistoryForwardListCount())
    return false;
  if (offset < -webview->Client()->HistoryBackListCount())
    return false;
  web_frame_->Scheduler()->WillNavigateBackForwardSoon();
  webview->Client()->NavigateBackForwardSoon(offset);
  return true;
}

void LocalFrameClientImpl::DidAccessInitialDocument() {
  if (web_frame_->Client())
    web_frame_->Client()->DidAccessInitialDocument();
}

void LocalFrameClientImpl::DidDisplayInsecureContent() {
  if (web_frame_->Client())
    web_frame_->Client()->DidDisplayInsecureContent();
}

void LocalFrameClientImpl::DidContainInsecureFormAction() {
  if (web_frame_->Client())
    web_frame_->Client()->DidContainInsecureFormAction();
}

void LocalFrameClientImpl::DidRunInsecureContent(SecurityOrigin* origin,
                                                 const KURL& insecure_url) {
  if (web_frame_->Client()) {
    web_frame_->Client()->DidRunInsecureContent(WebSecurityOrigin(origin),
                                                insecure_url);
  }
}

void LocalFrameClientImpl::DidDetectXSS(const KURL& insecure_url,
                                        bool did_block_entire_page) {
  if (web_frame_->Client())
    web_frame_->Client()->DidDetectXSS(insecure_url, did_block_entire_page);
}

void LocalFrameClientImpl::DidDispatchPingLoader(const KURL& url) {
  if (web_frame_->Client())
    web_frame_->Client()->DidDispatchPingLoader(url);
}

void LocalFrameClientImpl::DidDisplayContentWithCertificateErrors(
    const KURL& url) {
  if (web_frame_->Client())
    web_frame_->Client()->DidDisplayContentWithCertificateErrors(url);
}

void LocalFrameClientImpl::DidRunContentWithCertificateErrors(const KURL& url) {
  if (web_frame_->Client())
    web_frame_->Client()->DidRunContentWithCertificateErrors(url);
}

void LocalFrameClientImpl::ReportLegacySymantecCert(const KURL& url,
                                                    Time cert_validity_start) {
  // To prevent log spam, only log the message once per hostname.
  if (certificate_warning_hosts_.Contains(url.Host()))
    return;

  // After |kMaxCertificateWarningMessages| warnings, stop printing messages to
  // the console. At exactly |kMaxCertificateWarningMessages| warnings, print a
  // message that additional resources on the page use legacy certificates
  // without specifying which exact resources. Before
  // |kMaxCertificateWarningMessages| messages, print the exact resource URL in
  // the message to help the developer pinpoint the problematic resources.

  if (num_certificate_warning_messages_ > kMaxCertificateWarningMessages)
    return;

  WebString console_message;

  if (num_certificate_warning_messages_ == kMaxCertificateWarningMessages) {
    console_message =
        WebString(String("Additional resources on this page were loaded with "
                         "SSL certificates that will be "
                         "distrusted in the future. "
                         "Once distrusted, users will be prevented from "
                         "loading these resources. See"
                         "https://g.co/chrome/symantecpkicerts for "
                         "more information."));
  } else if (!web_frame_->Client()->OverrideLegacySymantecCertConsoleMessage(
                 url, cert_validity_start, &console_message)) {
    console_message = WebString(
        String::Format("The SSL certificate used to load resources from %s"
                       " will be "
                       "distrusted in the future. "
                       "Once distrusted, users will be prevented from "
                       "loading these resources. See "
                       "https://g.co/chrome/symantecpkicerts for "
                       "more information.",
                       SecurityOrigin::Create(url)->ToString().Utf8().data()));
  }
  num_certificate_warning_messages_++;
  certificate_warning_hosts_.insert(url.Host());
  // To avoid spamming the console, use Verbose message level for subframe
  // resources and only use the warning level for main-frame resources.
  web_frame_->GetFrame()->GetDocument()->AddConsoleMessage(
      ConsoleMessage::Create(
          kSecurityMessageSource,
          web_frame_->Parent() ? kVerboseMessageLevel : kWarningMessageLevel,
          console_message));
}

void LocalFrameClientImpl::DidChangePerformanceTiming() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangePerformanceTiming();
}

void LocalFrameClientImpl::DidObserveLoadingBehavior(
    WebLoadingBehaviorFlag behavior) {
  if (web_frame_->Client())
    web_frame_->Client()->DidObserveLoadingBehavior(behavior);
}

void LocalFrameClientImpl::DidObserveNewFeatureUsage(
    mojom::WebFeature feature) {
  if (web_frame_->Client())
    web_frame_->Client()->DidObserveNewFeatureUsage(feature);
}

void LocalFrameClientImpl::SelectorMatchChanged(
    const Vector<String>& added_selectors,
    const Vector<String>& removed_selectors) {
  if (WebFrameClient* client = web_frame_->Client()) {
    client->DidMatchCSS(WebVector<WebString>(added_selectors),
                        WebVector<WebString>(removed_selectors));
  }
}

DocumentLoader* LocalFrameClientImpl::CreateDocumentLoader(
    LocalFrame* frame,
    const ResourceRequest& request,
    const SubstituteData& data,
    ClientRedirectPolicy client_redirect_policy) {
  DCHECK(frame);

  WebDocumentLoaderImpl* document_loader = WebDocumentLoaderImpl::Create(
      frame, request, data, client_redirect_policy);
  if (web_frame_->Client())
    web_frame_->Client()->DidCreateDocumentLoader(document_loader);
  return document_loader;
}

String LocalFrameClientImpl::UserAgent() {
  WebString override =
      web_frame_->Client() ? web_frame_->Client()->UserAgentOverride() : "";
  if (!override.IsEmpty())
    return override;

  if (user_agent_.IsEmpty())
    user_agent_ = Platform::Current()->UserAgent();
  return user_agent_;
}

String LocalFrameClientImpl::DoNotTrackValue() {
  WebString do_not_track = web_frame_->Client()->DoNotTrackValue();
  if (!do_not_track.IsEmpty())
    return do_not_track;
  return String();
}

// Called when the FrameLoader goes into a state in which a new page load
// will occur.
void LocalFrameClientImpl::TransitionToCommittedForNewPage() {
  web_frame_->CreateFrameView();
}

LocalFrame* LocalFrameClientImpl::CreateFrame(
    const AtomicString& name,
    HTMLFrameOwnerElement* owner_element) {
  return web_frame_->CreateChildFrame(name, owner_element);
}

bool LocalFrameClientImpl::CanCreatePluginWithoutRenderer(
    const String& mime_type) const {
  if (!web_frame_->Client())
    return false;

  return web_frame_->Client()->CanCreatePluginWithoutRenderer(mime_type);
}

PluginView* LocalFrameClientImpl::CreatePlugin(
    HTMLPlugInElement& element,
    const KURL& url,
    const Vector<String>& param_names,
    const Vector<String>& param_values,
    const String& mime_type,
    bool load_manually,
    DetachedPluginPolicy policy) {
  if (!web_frame_->Client())
    return nullptr;

  WebPluginParams params;
  params.url = url;
  params.mime_type = mime_type;
  params.attribute_names = param_names;
  params.attribute_values = param_values;
  params.load_manually = load_manually;

  WebPlugin* web_plugin = web_frame_->Client()->CreatePlugin(params);
  if (!web_plugin)
    return nullptr;

  // The container takes ownership of the WebPlugin.
  WebPluginContainerImpl* container =
      WebPluginContainerImpl::Create(element, web_plugin);

  if (!web_plugin->Initialize(container))
    return nullptr;

  if (policy != kAllowDetachedPlugin && !element.GetLayoutObject())
    return nullptr;

  return container;
}

std::unique_ptr<WebMediaPlayer> LocalFrameClientImpl::CreateWebMediaPlayer(
    HTMLMediaElement& html_media_element,
    const WebMediaPlayerSource& source,
    WebMediaPlayerClient* client,
    WebLayerTreeView* layer_tree_view) {
  WebLocalFrameImpl* web_frame =
      WebLocalFrameImpl::FromFrame(html_media_element.GetDocument().GetFrame());

  if (!web_frame || !web_frame->Client())
    return nullptr;

  return CoreInitializer::GetInstance().CreateWebMediaPlayer(
      web_frame->Client(), html_media_element, source, client, layer_tree_view);
}

WebRemotePlaybackClient* LocalFrameClientImpl::CreateWebRemotePlaybackClient(
    HTMLMediaElement& html_media_element) {
  return CoreInitializer::GetInstance().CreateWebRemotePlaybackClient(
      html_media_element);
}

WebCookieJar* LocalFrameClientImpl::CookieJar() const {
  if (!web_frame_->Client())
    return nullptr;
  return web_frame_->Client()->CookieJar();
}

void LocalFrameClientImpl::FrameFocused() const {
  if (web_frame_->Client())
    web_frame_->Client()->FrameFocused();
}

void LocalFrameClientImpl::DidChangeName(const String& name) {
  if (!web_frame_->Client())
    return;
  web_frame_->Client()->DidChangeName(name);
}

void LocalFrameClientImpl::DidEnforceInsecureRequestPolicy(
    WebInsecureRequestPolicy policy) {
  if (!web_frame_->Client())
    return;
  web_frame_->Client()->DidEnforceInsecureRequestPolicy(policy);
}

void LocalFrameClientImpl::DidUpdateToUniqueOrigin() {
  if (!web_frame_->Client())
    return;
  DCHECK(web_frame_->GetSecurityOrigin().IsUnique());
  web_frame_->Client()->DidUpdateToUniqueOrigin(
      web_frame_->GetSecurityOrigin().IsPotentiallyTrustworthy());
}

void LocalFrameClientImpl::DidChangeFramePolicy(
    Frame* child_frame,
    SandboxFlags flags,
    const WebParsedFeaturePolicy& container_policy) {
  if (!web_frame_->Client())
    return;
  web_frame_->Client()->DidChangeFramePolicy(
      WebFrame::FromFrame(child_frame), static_cast<WebSandboxFlags>(flags),
      container_policy);
}

void LocalFrameClientImpl::DidSetFeaturePolicyHeader(
    const WebParsedFeaturePolicy& parsed_header) {
  if (web_frame_->Client())
    web_frame_->Client()->DidSetFeaturePolicyHeader(parsed_header);
}

void LocalFrameClientImpl::DidAddContentSecurityPolicies(
    const blink::WebVector<WebContentSecurityPolicy>& policies) {
  if (web_frame_->Client())
    web_frame_->Client()->DidAddContentSecurityPolicies(policies);
}

void LocalFrameClientImpl::DidChangeFrameOwnerProperties(
    HTMLFrameOwnerElement* frame_element) {
  if (!web_frame_->Client())
    return;

  web_frame_->Client()->DidChangeFrameOwnerProperties(
      WebFrame::FromFrame(frame_element->ContentFrame()),
      WebFrameOwnerProperties(
          frame_element->BrowsingContextContainerName(),
          frame_element->ScrollingMode(), frame_element->MarginWidth(),
          frame_element->MarginHeight(), frame_element->AllowFullscreen(),
          frame_element->AllowPaymentRequest(), frame_element->IsDisplayNone(),
          frame_element->Csp()));
}

void LocalFrameClientImpl::DispatchWillStartUsingPeerConnectionHandler(
    WebRTCPeerConnectionHandler* handler) {
  web_frame_->Client()->WillStartUsingPeerConnectionHandler(handler);
}

bool LocalFrameClientImpl::ShouldBlockWebGL() {
  DCHECK(web_frame_->Client());
  return web_frame_->Client()->ShouldBlockWebGL();
}

void LocalFrameClientImpl::DispatchWillInsertBody() {
  if (web_frame_->Client())
    web_frame_->Client()->WillInsertBody();
}

std::unique_ptr<WebServiceWorkerProvider>
LocalFrameClientImpl::CreateServiceWorkerProvider() {
  if (!web_frame_->Client())
    return nullptr;
  return web_frame_->Client()->CreateServiceWorkerProvider();
}

ContentSettingsClient& LocalFrameClientImpl::GetContentSettingsClient() {
  return web_frame_->GetContentSettingsClient();
}

SharedWorkerRepositoryClient*
LocalFrameClientImpl::GetSharedWorkerRepositoryClient() {
  return web_frame_->SharedWorkerRepositoryClient();
}

std::unique_ptr<WebApplicationCacheHost>
LocalFrameClientImpl::CreateApplicationCacheHost(
    WebApplicationCacheHostClient* client) {
  if (!web_frame_->Client())
    return nullptr;
  return web_frame_->Client()->CreateApplicationCacheHost(client);
}

void LocalFrameClientImpl::DispatchDidChangeManifest() {
  if (web_frame_->Client())
    web_frame_->Client()->DidChangeManifest();
}

unsigned LocalFrameClientImpl::BackForwardLength() {
  WebViewImpl* webview = web_frame_->ViewImpl();
  if (!webview || !webview->Client())
    return 0;
  return webview->Client()->HistoryBackListCount() + 1 +
         webview->Client()->HistoryForwardListCount();
}

void LocalFrameClientImpl::SuddenTerminationDisablerChanged(
    bool present,
    WebSuddenTerminationDisablerType type) {
  if (web_frame_->Client()) {
    web_frame_->Client()->SuddenTerminationDisablerChanged(present, type);
  }
}

BlameContext* LocalFrameClientImpl::GetFrameBlameContext() {
  if (WebFrameClient* client = web_frame_->Client())
    return client->GetFrameBlameContext();
  return nullptr;
}

WebEffectiveConnectionType LocalFrameClientImpl::GetEffectiveConnectionType() {
  if (web_frame_->Client())
    return web_frame_->Client()->GetEffectiveConnectionType();
  return WebEffectiveConnectionType::kTypeUnknown;
}

void LocalFrameClientImpl::SetEffectiveConnectionTypeForTesting(
    WebEffectiveConnectionType type) {
  if (web_frame_->Client())
    return web_frame_->Client()->SetEffectiveConnectionTypeForTesting(type);
}

bool LocalFrameClientImpl::IsClientLoFiActiveForFrame() {
  if (web_frame_->Client())
    return web_frame_->Client()->IsClientLoFiActiveForFrame();
  return false;
}

bool LocalFrameClientImpl::ShouldUseClientLoFiForRequest(
    const ResourceRequest& request) {
  if (web_frame_->Client()) {
    return web_frame_->Client()->ShouldUseClientLoFiForRequest(
        WrappedResourceRequest(request));
  }
  return false;
}

WebDevToolsAgentImpl* LocalFrameClientImpl::DevToolsAgent() {
  return WebLocalFrameImpl::FromFrame(web_frame_->GetFrame()->LocalFrameRoot())
      ->DevToolsAgentImpl();
}

KURL LocalFrameClientImpl::OverrideFlashEmbedWithHTML(const KURL& url) {
  return web_frame_->Client()->OverrideFlashEmbedWithHTML(WebURL(url));
}

void LocalFrameClientImpl::SetHasReceivedUserGesture(bool received_previously) {
  // The client potentially needs to dispatch the event to other processes only
  // for the first time.
  //
  // TODO(mustaq): Can we remove |received_previously|, specially when
  // autofill-client below ignores it already? crbug.com/775930
  if (!received_previously && web_frame_->Client())
    web_frame_->Client()->SetHasReceivedUserGesture();
  // WebAutofillClient reacts only to the user gestures for this particular
  // frame. |received_previously| is ignored because it may be true due to an
  // event in a child frame.
  if (WebAutofillClient* autofill_client = web_frame_->AutofillClient())
    autofill_client->UserGestureObserved();
}

void LocalFrameClientImpl::AbortClientNavigation() {
  if (web_frame_->Client())
    web_frame_->Client()->AbortClientNavigation();
}

WebSpellCheckPanelHostClient* LocalFrameClientImpl::SpellCheckPanelHostClient()
    const {
  return web_frame_->SpellCheckPanelHostClient();
}

TextCheckerClient& LocalFrameClientImpl::GetTextCheckerClient() const {
  return web_frame_->GetTextCheckerClient();
}

std::unique_ptr<blink::WebURLLoader> LocalFrameClientImpl::CreateURLLoader(
    const ResourceRequest& request,
    scoped_refptr<WebTaskRunner> task_runner) {
  WrappedResourceRequest wrapped(request);
  return web_frame_->CreateURLLoader(wrapped,
                                     task_runner->ToSingleThreadTaskRunner());
}

service_manager::InterfaceProvider*
LocalFrameClientImpl::GetInterfaceProvider() {
  return web_frame_->Client()->GetInterfaceProvider();
}

void LocalFrameClientImpl::AnnotatedRegionsChanged() {
  web_frame_->Client()->DraggableRegionsChanged();
}

void LocalFrameClientImpl::DidBlockFramebust(const KURL& url) {
  web_frame_->Client()->DidBlockFramebust(url);
}

String LocalFrameClientImpl::GetInstrumentationToken() {
  return web_frame_->Client()->GetInstrumentationToken();
}

void LocalFrameClientImpl::ScrollRectToVisibleInParentFrame(
    const WebRect& rect_to_scroll,
    const WebRemoteScrollProperties& properties) {
  web_frame_->Client()->ScrollRectToVisibleInParentFrame(rect_to_scroll,
                                                         properties);
}

}  // namespace blink
