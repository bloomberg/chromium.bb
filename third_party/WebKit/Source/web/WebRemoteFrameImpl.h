// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameImpl_h
#define WebRemoteFrameImpl_h

#include "core/frame/RemoteFrame.h"
#include "core/frame/WebRemoteFrameBase.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Compiler.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/web/WebRemoteFrameClient.h"
#include "web/RemoteFrameClientImpl.h"
#include "web/WebExport.h"

namespace blink {

class FrameOwner;
class RemoteFrame;
enum class WebFrameLoadType;
class WebAssociatedURLLoader;
struct WebAssociatedURLLoaderOptions;

class WEB_EXPORT WebRemoteFrameImpl final
    : NON_EXPORTED_BASE(public WebRemoteFrameBase) {
 public:
  static WebRemoteFrameImpl* Create(WebTreeScopeType,
                                    WebRemoteFrameClient*,
                                    WebFrame* opener = nullptr);
  ~WebRemoteFrameImpl() override;

  // WebFrame methods:
  void Close() override;
  WebString AssignedName() const override;
  void SetName(const WebString&) override;
  WebVector<WebIconURL> IconURLs(int icon_types_mask) const override;
  void SetSharedWorkerRepositoryClient(
      WebSharedWorkerRepositoryClient*) override;
  void SetCanHaveScrollbars(bool) override;
  WebSize GetScrollOffset() const override;
  void SetScrollOffset(const WebSize&) override;
  WebSize ContentsSize() const override;
  bool HasVisibleContent() const override;
  WebRect VisibleContentRect() const override;
  bool HasHorizontalScrollbar() const override;
  bool HasVerticalScrollbar() const override;
  WebView* View() const override;
  WebDocument GetDocument() const override;
  WebPerformance Performance() const override;
  void DispatchUnloadEvent() override;
  void ExecuteScript(const WebScriptSource&) override;
  void ExecuteScriptInIsolatedWorld(int world_id,
                                    const WebScriptSource* sources,
                                    unsigned num_sources) override;
  void SetIsolatedWorldSecurityOrigin(int world_id,
                                      const WebSecurityOrigin&) override;
  void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                             const WebString&) override;
  void CollectGarbage() override;
  v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      const WebScriptSource&) override;
  void ExecuteScriptInIsolatedWorld(
      int world_id,
      const WebScriptSource* sources_in,
      unsigned num_sources,
      WebVector<v8::Local<v8::Value>>* results) override;
  v8::Local<v8::Value> CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>,
      v8::Local<v8::Value>,
      int argc,
      v8::Local<v8::Value> argv[]) override;
  v8::Local<v8::Context> MainWorldScriptContext() const override;
  void Reload(WebFrameLoadType) override;
  void ReloadWithOverrideURL(const WebURL& override_url,
                             WebFrameLoadType) override;
  void LoadRequest(const WebURLRequest&) override;
  void LoadHTMLString(const WebData& html,
                      const WebURL& base_url,
                      const WebURL& unreachable_url,
                      bool replace) override;
  void StopLoading() override;
  WebDataSource* ProvisionalDataSource() const override;
  WebDataSource* DataSource() const override;
  void EnableViewSourceMode(bool enable) override;
  bool IsViewSourceModeEnabled() const override;
  void SetReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
  WebAssociatedURLLoader* CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions&) override;
  unsigned UnloadListenerCount() const override;
  int PrintBegin(const WebPrintParams&,
                 const WebNode& constrain_to_node) override;
  float PrintPage(int page_to_print, WebCanvas*) override;
  float GetPrintPageShrink(int page) override;
  void PrintEnd() override;
  bool IsPrintScalingDisabledForPlugin(const WebNode&) override;
  void PrintPagesWithBoundaries(WebCanvas*, const WebSize&) override;
  void DispatchMessageEventWithOriginCheck(
      const WebSecurityOrigin& intended_target_origin,
      const WebDOMEvent&) override;

  WebRect SelectionBoundsRect() const override;

  WebString LayerTreeAsText(bool show_debug_info = false) const override;

  // WebRemoteFrame methods:
  WebLocalFrame* CreateLocalChild(WebTreeScopeType,
                                  const WebString& name,
                                  WebSandboxFlags,
                                  WebFrameClient*,
                                  blink::InterfaceProvider*,
                                  blink::InterfaceRegistry*,
                                  WebFrame* previous_sibling,
                                  const WebParsedFeaturePolicy&,
                                  const WebFrameOwnerProperties&,
                                  WebFrame* opener) override;
  WebRemoteFrame* CreateRemoteChild(WebTreeScopeType,
                                    const WebString& name,
                                    WebSandboxFlags,
                                    const WebParsedFeaturePolicy&,
                                    WebRemoteFrameClient*,
                                    WebFrame* opener) override;
  void SetWebLayer(WebLayer*) override;
  void SetReplicatedOrigin(const WebSecurityOrigin&) override;
  void SetReplicatedSandboxFlags(WebSandboxFlags) override;
  void SetReplicatedName(const WebString&) override;
  void SetReplicatedFeaturePolicyHeader(
      const WebParsedFeaturePolicy& parsed_header) override;
  void AddReplicatedContentSecurityPolicyHeader(
      const WebString& header_value,
      WebContentSecurityPolicyType,
      WebContentSecurityPolicySource) override;
  void ResetReplicatedContentSecurityPolicy() override;
  void SetReplicatedInsecureRequestPolicy(WebInsecureRequestPolicy) override;
  void SetReplicatedPotentiallyTrustworthyUniqueOrigin(bool) override;
  void DispatchLoadEventOnFrameOwner() override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  bool IsIgnoredForHitTest() const override;
  void WillEnterFullscreen() override;
  void SetHasReceivedUserGesture() override;
  v8::Local<v8::Object> GlobalProxy() const override;

  void InitializeCoreFrame(Page&,
                           FrameOwner*,
                           const AtomicString& name) override;
  RemoteFrame* GetFrame() const override { return frame_.Get(); }

  void SetCoreFrame(RemoteFrame*);

  WebRemoteFrameClient* Client() const { return client_; }

  static WebRemoteFrameImpl* FromFrame(RemoteFrame&);

  DECLARE_TRACE();

 private:
  WebRemoteFrameImpl(WebTreeScopeType, WebRemoteFrameClient*);

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebRemoteFrameImpl.
  bool IsWebLocalFrame() const override;
  WebLocalFrame* ToWebLocalFrame() override;
  bool IsWebRemoteFrame() const override;
  WebRemoteFrame* ToWebRemoteFrame() override;

  Member<RemoteFrameClientImpl> frame_client_;
  Member<RemoteFrame> frame_;
  WebRemoteFrameClient* client_;

  // Oilpan: WebRemoteFrameImpl must remain alive until close() is called.
  // Accomplish that by keeping a self-referential Persistent<>. It is
  // cleared upon close().
  SelfKeepAlive<WebRemoteFrameImpl> self_keep_alive_;
};

DEFINE_TYPE_CASTS(WebRemoteFrameImpl,
                  WebFrame,
                  frame,
                  frame->IsWebRemoteFrame(),
                  frame.IsWebRemoteFrame());

}  // namespace blink

#endif  // WebRemoteFrameImpl_h
