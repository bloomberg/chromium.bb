// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameImpl_h
#define WebRemoteFrameImpl_h

#include "core/CoreExport.h"
#include "core/frame/RemoteFrame.h"
#include "platform/heap/SelfKeepAlive.h"
#include "platform/wtf/Compiler.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/web/WebRemoteFrame.h"
#include "public/web/WebRemoteFrameClient.h"

namespace blink {

class FrameOwner;
class RemoteFrame;
class RemoteFrameClientImpl;
enum class WebFrameLoadType;
class WebView;

class CORE_EXPORT WebRemoteFrameImpl final
    : public GarbageCollectedFinalized<WebRemoteFrameImpl>,
      public WebRemoteFrame {
 public:
  static WebRemoteFrameImpl* Create(WebTreeScopeType, WebRemoteFrameClient*);
  static WebRemoteFrameImpl* CreateMainFrame(WebView*,
                                             WebRemoteFrameClient*,
                                             WebFrame* opener = nullptr);

  ~WebRemoteFrameImpl() override;

  // WebFrame methods:
  void Close() override;
  WebRect VisibleContentRect() const override;
  WebView* View() const override;
  void StopLoading() override;

  // WebRemoteFrame methods:
  WebLocalFrame* CreateLocalChild(WebTreeScopeType,
                                  const WebString& name,
                                  WebSandboxFlags,
                                  WebFrameClient*,
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

  void InitializeCoreFrame(Page&, FrameOwner*, const AtomicString& name);
  RemoteFrame* GetFrame() const { return frame_.Get(); }

  void SetCoreFrame(RemoteFrame*);

  WebRemoteFrameClient* Client() const { return client_; }

  static WebRemoteFrameImpl* FromFrame(RemoteFrame&);

  void Trace(blink::Visitor*);

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
