// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/WebRemoteFrameImpl.h"

#include "bindings/core/v8/WindowProxy.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/RemoteSecurityContext.h"
#include "core/dom/SecurityContext.h"
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutObject.h"
#include "core/page/Page.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebFeaturePolicy.h"
#include "public/platform/WebFloatRect.h"
#include "public/platform/WebRect.h"
#include "public/web/WebDocument.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebPerformance.h"
#include "public/web/WebRange.h"
#include "public/web/WebTreeScopeType.h"
#include "v8/include/v8.h"
#include "web/RemoteFrameOwner.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

WebRemoteFrame* WebRemoteFrame::Create(WebTreeScopeType scope,
                                       WebRemoteFrameClient* client,
                                       WebFrame* opener) {
  return WebRemoteFrameImpl::Create(scope, client, opener);
}

WebRemoteFrameImpl* WebRemoteFrameImpl::Create(WebTreeScopeType scope,
                                               WebRemoteFrameClient* client,
                                               WebFrame* opener) {
  WebRemoteFrameImpl* frame = new WebRemoteFrameImpl(scope, client);
  frame->SetOpener(opener);
  return frame;
}

WebRemoteFrameImpl::~WebRemoteFrameImpl() {}

DEFINE_TRACE(WebRemoteFrameImpl) {
  visitor->Trace(frame_client_);
  visitor->Trace(frame_);
  WebRemoteFrameBase::Trace(visitor);
  WebFrame::TraceFrames(visitor, this);
}

bool WebRemoteFrameImpl::IsWebLocalFrame() const {
  return false;
}

WebLocalFrame* WebRemoteFrameImpl::ToWebLocalFrame() {
  NOTREACHED();
  return nullptr;
}

bool WebRemoteFrameImpl::IsWebRemoteFrame() const {
  return true;
}

WebRemoteFrame* WebRemoteFrameImpl::ToWebRemoteFrame() {
  return this;
}

void WebRemoteFrameImpl::Close() {
  WebRemoteFrame::Close();

  self_keep_alive_.Clear();
}

WebString WebRemoteFrameImpl::AssignedName() const {
  NOTREACHED();
  return WebString();
}

void WebRemoteFrameImpl::SetName(const WebString&) {
  NOTREACHED();
}

WebVector<WebIconURL> WebRemoteFrameImpl::IconURLs(int icon_types_mask) const {
  NOTREACHED();
  return WebVector<WebIconURL>();
}

void WebRemoteFrameImpl::SetSharedWorkerRepositoryClient(
    WebSharedWorkerRepositoryClient*) {
  NOTREACHED();
}

void WebRemoteFrameImpl::SetCanHaveScrollbars(bool) {
  NOTREACHED();
}

WebSize WebRemoteFrameImpl::GetScrollOffset() const {
  NOTREACHED();
  return WebSize();
}

void WebRemoteFrameImpl::SetScrollOffset(const WebSize&) {
  NOTREACHED();
}

WebSize WebRemoteFrameImpl::ContentsSize() const {
  NOTREACHED();
  return WebSize();
}

bool WebRemoteFrameImpl::HasVisibleContent() const {
  NOTREACHED();
  return false;
}

WebRect WebRemoteFrameImpl::VisibleContentRect() const {
  NOTREACHED();
  return WebRect();
}

bool WebRemoteFrameImpl::HasHorizontalScrollbar() const {
  NOTREACHED();
  return false;
}

bool WebRemoteFrameImpl::HasVerticalScrollbar() const {
  NOTREACHED();
  return false;
}

WebView* WebRemoteFrameImpl::View() const {
  if (!GetFrame())
    return nullptr;
  return WebViewBase::FromPage(GetFrame()->GetPage());
}

WebDocument WebRemoteFrameImpl::GetDocument() const {
  // TODO(dcheng): this should also ASSERT_NOT_REACHED, but a lot of
  // code tries to access the document of a remote frame at the moment.
  return WebDocument();
}

WebPerformance WebRemoteFrameImpl::Performance() const {
  NOTREACHED();
  return WebPerformance();
}

void WebRemoteFrameImpl::DispatchUnloadEvent() {
  NOTREACHED();
}

void WebRemoteFrameImpl::ExecuteScript(const WebScriptSource&) {
  NOTREACHED();
}

void WebRemoteFrameImpl::ExecuteScriptInIsolatedWorld(
    int world_id,
    const WebScriptSource* sources,
    unsigned num_sources) {
  NOTREACHED();
}

void WebRemoteFrameImpl::SetIsolatedWorldSecurityOrigin(
    int world_id,
    const WebSecurityOrigin&) {
  NOTREACHED();
}

void WebRemoteFrameImpl::SetIsolatedWorldContentSecurityPolicy(
    int world_id,
    const WebString&) {
  NOTREACHED();
}

void WebRemoteFrameImpl::CollectGarbage() {
  NOTREACHED();
}

v8::Local<v8::Value> WebRemoteFrameImpl::ExecuteScriptAndReturnValue(
    const WebScriptSource&) {
  NOTREACHED();
  return v8::Local<v8::Value>();
}

void WebRemoteFrameImpl::ExecuteScriptInIsolatedWorld(
    int world_id,
    const WebScriptSource* sources_in,
    unsigned num_sources,
    WebVector<v8::Local<v8::Value>>* results) {
  NOTREACHED();
}

v8::Local<v8::Value> WebRemoteFrameImpl::CallFunctionEvenIfScriptDisabled(
    v8::Local<v8::Function>,
    v8::Local<v8::Value>,
    int argc,
    v8::Local<v8::Value> argv[]) {
  NOTREACHED();
  return v8::Local<v8::Value>();
}

v8::Local<v8::Context> WebRemoteFrameImpl::MainWorldScriptContext() const {
  NOTREACHED();
  return v8::Local<v8::Context>();
}

void WebRemoteFrameImpl::Reload(WebFrameLoadType) {
  NOTREACHED();
}

void WebRemoteFrameImpl::ReloadWithOverrideURL(const WebURL& override_url,
                                               WebFrameLoadType) {
  NOTREACHED();
}

void WebRemoteFrameImpl::LoadRequest(const WebURLRequest&) {
  NOTREACHED();
}

void WebRemoteFrameImpl::LoadHTMLString(const WebData& html,
                                        const WebURL& base_url,
                                        const WebURL& unreachable_url,
                                        bool replace) {
  NOTREACHED();
}

void WebRemoteFrameImpl::StopLoading() {
  // TODO(dcheng,japhet): Calling this method should stop loads
  // in all subframes, both remote and local.
}

WebDataSource* WebRemoteFrameImpl::ProvisionalDataSource() const {
  NOTREACHED();
  return nullptr;
}

WebDataSource* WebRemoteFrameImpl::DataSource() const {
  NOTREACHED();
  return nullptr;
}

void WebRemoteFrameImpl::EnableViewSourceMode(bool enable) {
  NOTREACHED();
}

bool WebRemoteFrameImpl::IsViewSourceModeEnabled() const {
  NOTREACHED();
  return false;
}

void WebRemoteFrameImpl::SetReferrerForRequest(WebURLRequest&,
                                               const WebURL& referrer) {
  NOTREACHED();
}

WebAssociatedURLLoader* WebRemoteFrameImpl::CreateAssociatedURLLoader(
    const WebAssociatedURLLoaderOptions&) {
  NOTREACHED();
  return nullptr;
}

unsigned WebRemoteFrameImpl::UnloadListenerCount() const {
  NOTREACHED();
  return 0;
}

int WebRemoteFrameImpl::PrintBegin(const WebPrintParams&,
                                   const WebNode& constrain_to_node) {
  NOTREACHED();
  return 0;
}

float WebRemoteFrameImpl::PrintPage(int page_to_print, WebCanvas*) {
  NOTREACHED();
  return 0.0;
}

float WebRemoteFrameImpl::GetPrintPageShrink(int page) {
  NOTREACHED();
  return 0.0;
}

void WebRemoteFrameImpl::PrintEnd() {
  NOTREACHED();
}

bool WebRemoteFrameImpl::IsPrintScalingDisabledForPlugin(const WebNode&) {
  NOTREACHED();
  return false;
}

void WebRemoteFrameImpl::PrintPagesWithBoundaries(WebCanvas*, const WebSize&) {
  NOTREACHED();
}

void WebRemoteFrameImpl::DispatchMessageEventWithOriginCheck(
    const WebSecurityOrigin& intended_target_origin,
    const WebDOMEvent&) {
  NOTREACHED();
}

WebRect WebRemoteFrameImpl::SelectionBoundsRect() const {
  NOTREACHED();
  return WebRect();
}

WebString WebRemoteFrameImpl::LayerTreeAsText(bool show_debug_info) const {
  NOTREACHED();
  return WebString();
}

WebLocalFrame* WebRemoteFrameImpl::CreateLocalChild(
    WebTreeScopeType scope,
    const WebString& name,
    WebSandboxFlags sandbox_flags,
    WebFrameClient* client,
    blink::InterfaceProvider* interface_provider,
    blink::InterfaceRegistry* interface_registry,
    WebFrame* previous_sibling,
    const WebParsedFeaturePolicy& container_policy,
    const WebFrameOwnerProperties& frame_owner_properties,
    WebFrame* opener) {
  WebLocalFrameImpl* child = WebLocalFrameImpl::Create(
      scope, client, interface_provider, interface_registry, opener);
  InsertAfter(child, previous_sibling);
  RemoteFrameOwner* owner =
      RemoteFrameOwner::Create(static_cast<SandboxFlags>(sandbox_flags),
                               container_policy, frame_owner_properties);
  child->InitializeCoreFrame(*GetFrame()->GetPage(), owner, name);
  DCHECK(child->GetFrame());
  return child;
}

void WebRemoteFrameImpl::InitializeCoreFrame(Page& page,
                                             FrameOwner* owner,
                                             const AtomicString& name) {
  SetCoreFrame(RemoteFrame::Create(frame_client_.Get(), page, owner));
  GetFrame()->CreateView();
  frame_->Tree().SetName(name);
}

WebRemoteFrame* WebRemoteFrameImpl::CreateRemoteChild(
    WebTreeScopeType scope,
    const WebString& name,
    WebSandboxFlags sandbox_flags,
    const WebParsedFeaturePolicy& container_policy,
    WebRemoteFrameClient* client,
    WebFrame* opener) {
  WebRemoteFrameImpl* child = WebRemoteFrameImpl::Create(scope, client, opener);
  AppendChild(child);
  RemoteFrameOwner* owner =
      RemoteFrameOwner::Create(static_cast<SandboxFlags>(sandbox_flags),
                               container_policy, WebFrameOwnerProperties());
  child->InitializeCoreFrame(*GetFrame()->GetPage(), owner, name);
  return child;
}

void WebRemoteFrameImpl::SetWebLayer(WebLayer* layer) {
  if (!GetFrame())
    return;

  GetFrame()->SetWebLayer(layer);
}

void WebRemoteFrameImpl::SetCoreFrame(RemoteFrame* frame) {
  frame_ = frame;
}

WebRemoteFrameImpl* WebRemoteFrameImpl::FromFrame(RemoteFrame& frame) {
  if (!frame.Client())
    return nullptr;
  return static_cast<RemoteFrameClientImpl*>(frame.Client())->GetWebFrame();
}

void WebRemoteFrameImpl::SetReplicatedOrigin(const WebSecurityOrigin& origin) {
  DCHECK(GetFrame());
  GetFrame()->GetSecurityContext()->SetReplicatedOrigin(origin);

  // If the origin of a remote frame changed, the accessibility object for the
  // owner element now points to a different child.
  //
  // TODO(dmazzoni, dcheng): there's probably a better way to solve this.
  // Run SitePerProcessAccessibilityBrowserTest.TwoCrossSiteNavigations to
  // ensure an alternate fix works.  http://crbug.com/566222
  FrameOwner* owner = GetFrame()->Owner();
  if (owner && owner->IsLocal()) {
    HTMLElement* owner_element = ToHTMLFrameOwnerElement(owner);
    AXObjectCache* cache = owner_element->GetDocument().ExistingAXObjectCache();
    if (cache)
      cache->ChildrenChanged(owner_element);
  }
}

void WebRemoteFrameImpl::SetReplicatedSandboxFlags(WebSandboxFlags flags) {
  DCHECK(GetFrame());
  GetFrame()->GetSecurityContext()->EnforceSandboxFlags(
      static_cast<SandboxFlags>(flags));
}

void WebRemoteFrameImpl::SetReplicatedName(const WebString& name) {
  DCHECK(GetFrame());
  GetFrame()->Tree().SetName(name);
}

void WebRemoteFrameImpl::SetReplicatedFeaturePolicyHeader(
    const WebParsedFeaturePolicy& parsed_header) {
  if (RuntimeEnabledFeatures::featurePolicyEnabled()) {
    WebFeaturePolicy* parent_feature_policy = nullptr;
    if (Parent()) {
      Frame* parent_frame = GetFrame()->Client()->Parent();
      parent_feature_policy =
          parent_frame->GetSecurityContext()->GetFeaturePolicy();
    }
    WebParsedFeaturePolicy container_policy;
    if (GetFrame()->Owner())
      container_policy = GetFrame()->Owner()->ContainerPolicy();
    GetFrame()->GetSecurityContext()->InitializeFeaturePolicy(
        parsed_header, container_policy, parent_feature_policy);
  }
}

void WebRemoteFrameImpl::AddReplicatedContentSecurityPolicyHeader(
    const WebString& header_value,
    WebContentSecurityPolicyType type,
    WebContentSecurityPolicySource source) {
  GetFrame()
      ->GetSecurityContext()
      ->GetContentSecurityPolicy()
      ->AddPolicyFromHeaderValue(
          header_value, static_cast<ContentSecurityPolicyHeaderType>(type),
          static_cast<ContentSecurityPolicyHeaderSource>(source));
}

void WebRemoteFrameImpl::ResetReplicatedContentSecurityPolicy() {
  GetFrame()->GetSecurityContext()->ResetReplicatedContentSecurityPolicy();
}

void WebRemoteFrameImpl::SetReplicatedInsecureRequestPolicy(
    WebInsecureRequestPolicy policy) {
  DCHECK(GetFrame());
  GetFrame()->GetSecurityContext()->SetInsecureRequestPolicy(policy);
}

void WebRemoteFrameImpl::SetReplicatedPotentiallyTrustworthyUniqueOrigin(
    bool is_unique_origin_potentially_trustworthy) {
  DCHECK(GetFrame());
  // If |isUniqueOriginPotentiallyTrustworthy| is true, then the origin must be
  // unique.
  DCHECK(!is_unique_origin_potentially_trustworthy ||
         GetFrame()->GetSecurityContext()->GetSecurityOrigin()->IsUnique());
  GetFrame()
      ->GetSecurityContext()
      ->GetSecurityOrigin()
      ->SetUniqueOriginIsPotentiallyTrustworthy(
          is_unique_origin_potentially_trustworthy);
}

void WebRemoteFrameImpl::DispatchLoadEventOnFrameOwner() {
  DCHECK(GetFrame()->Owner()->IsLocal());
  GetFrame()->Owner()->DispatchLoad();
}

void WebRemoteFrameImpl::DidStartLoading() {
  GetFrame()->SetIsLoading(true);
}

void WebRemoteFrameImpl::DidStopLoading() {
  GetFrame()->SetIsLoading(false);
  if (Parent() && Parent()->IsWebLocalFrame()) {
    WebLocalFrameImpl* parent_frame =
        ToWebLocalFrameImpl(Parent()->ToWebLocalFrame());
    parent_frame->GetFrame()->GetDocument()->CheckCompleted();
  }
}

bool WebRemoteFrameImpl::IsIgnoredForHitTest() const {
  HTMLFrameOwnerElement* owner = GetFrame()->DeprecatedLocalOwner();
  if (!owner || !owner->GetLayoutObject())
    return false;
  return owner->GetLayoutObject()->Style()->PointerEvents() ==
         EPointerEvents::kNone;
}

void WebRemoteFrameImpl::WillEnterFullscreen() {
  // This should only ever be called when the FrameOwner is local.
  HTMLFrameOwnerElement* owner_element =
      ToHTMLFrameOwnerElement(GetFrame()->Owner());

  // Call requestFullscreen() on |ownerElement| to make it the provisional
  // fullscreen element in FullscreenController, and to prepare
  // fullscreenchange events that will need to fire on it and its (local)
  // ancestors. The events will be triggered if/when fullscreen is entered.
  //
  // Passing |forCrossProcessAncestor| to requestFullscreen is necessary
  // because:
  // - |ownerElement| will need :-webkit-full-screen-ancestor style in
  //   addition to :-webkit-full-screen.
  // - there's no need to resend the ToggleFullscreen IPC to the browser
  //   process.
  //
  // TODO(alexmos): currently, this assumes prefixed requests, but in the
  // future, this should plumb in information about which request type
  // (prefixed or unprefixed) to use for firing fullscreen events.
  Fullscreen::RequestFullscreen(*owner_element,
                                Fullscreen::RequestType::kPrefixed,
                                true /* forCrossProcessAncestor */);
}

void WebRemoteFrameImpl::SetHasReceivedUserGesture() {
  GetFrame()->SetDocumentHasReceivedUserGesture();
}

v8::Local<v8::Object> WebRemoteFrameImpl::GlobalProxy() const {
  return GetFrame()
      ->GetWindowProxy(DOMWrapperWorld::MainWorld())
      ->GlobalProxyIfNotDetached();
}

WebRemoteFrameImpl::WebRemoteFrameImpl(WebTreeScopeType scope,
                                       WebRemoteFrameClient* client)
    : WebRemoteFrameBase(scope),
      frame_client_(RemoteFrameClientImpl::Create(this)),
      client_(client),
      self_keep_alive_(this) {}

}  // namespace blink
