// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrame_h
#define WebRemoteFrame_h

#include "public/platform/WebContentSecurityPolicy.h"
#include "public/platform/WebFeaturePolicy.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/web/WebFrame.h"
#include "public/web/WebSandboxFlags.h"
#include "v8/include/v8.h"

namespace blink {

enum class WebTreeScopeType;
class InterfaceProvider;
class InterfaceRegistry;
class WebFrameClient;
class WebLayer;
class WebRemoteFrameClient;
class WebString;
class WebView;

class WebRemoteFrame : public WebFrame {
 public:
  // Factory methods for creating a WebRemoteFrame. The WebRemoteFrameClient
  // argument must be non-null for all creation methods.
  BLINK_EXPORT static WebRemoteFrame* Create(WebTreeScopeType,
                                             WebRemoteFrameClient*);

  BLINK_EXPORT static WebRemoteFrame*
  CreateMainFrame(WebView*, WebRemoteFrameClient*, WebFrame* opener = nullptr);

  // Specialized factory methods to allow the embedder to replicate the frame
  // tree between processes.
  // TODO(dcheng): The embedder currently does not replicate local frames in
  // insertion order, so the local child version takes |previous_sibling| to
  // ensure that it is inserted into the correct location in the list of
  // children. If |previous_sibling| is null, the child is inserted at the
  // beginning.
  virtual WebLocalFrame* CreateLocalChild(WebTreeScopeType,
                                          const WebString& name,
                                          WebSandboxFlags,
                                          WebFrameClient*,
                                          blink::InterfaceProvider*,
                                          blink::InterfaceRegistry*,
                                          WebFrame* previous_sibling,
                                          const WebParsedFeaturePolicy&,
                                          const WebFrameOwnerProperties&,
                                          WebFrame* opener) = 0;

  virtual WebRemoteFrame* CreateRemoteChild(WebTreeScopeType,
                                            const WebString& name,
                                            WebSandboxFlags,
                                            const WebParsedFeaturePolicy&,
                                            WebRemoteFrameClient*,
                                            WebFrame* opener) = 0;

  // Layer for the in-process compositor.
  virtual void SetWebLayer(WebLayer*) = 0;

  // Set security origin replicated from another process.
  virtual void SetReplicatedOrigin(const WebSecurityOrigin&) = 0;

  // Set sandbox flags replicated from another process.
  virtual void SetReplicatedSandboxFlags(WebSandboxFlags) = 0;

  // Set frame |name| replicated from another process.
  virtual void SetReplicatedName(const WebString&) = 0;

  virtual void SetReplicatedFeaturePolicyHeader(
      const WebParsedFeaturePolicy& parsed_header) = 0;

  // Adds |header| to the set of replicated CSP headers.
  virtual void AddReplicatedContentSecurityPolicyHeader(
      const WebString& header_value,
      WebContentSecurityPolicyType,
      WebContentSecurityPolicySource) = 0;

  // Resets replicated CSP headers to an empty set.
  virtual void ResetReplicatedContentSecurityPolicy() = 0;

  // Set frame enforcement of insecure request policy replicated from another
  // process.
  virtual void SetReplicatedInsecureRequestPolicy(WebInsecureRequestPolicy) = 0;

  // Set the frame to a unique origin that is potentially trustworthy,
  // replicated from another process.
  virtual void SetReplicatedPotentiallyTrustworthyUniqueOrigin(bool) = 0;

  virtual void DispatchLoadEventOnFrameOwner() = 0;

  virtual void DidStartLoading() = 0;
  virtual void DidStopLoading() = 0;

  // Returns true if this frame should be ignored during hittesting.
  virtual bool IsIgnoredForHitTest() const = 0;

  // This is called in OOPIF scenarios when an element contained in this
  // frame is about to enter fullscreen.  This frame's owner
  // corresponds to the HTMLFrameOwnerElement to be fullscreened. Calling
  // this prepares FullscreenController to enter fullscreen for that frame
  // owner.
  virtual void WillEnterFullscreen() = 0;

  virtual void SetHasReceivedUserGesture() = 0;

 protected:
  explicit WebRemoteFrame(WebTreeScopeType scope) : WebFrame(scope) {}

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebRemoteFrame.
  bool IsWebLocalFrame() const override = 0;
  WebLocalFrame* ToWebLocalFrame() override = 0;
  bool IsWebRemoteFrame() const override = 0;
  WebRemoteFrame* ToWebRemoteFrame() override = 0;
};

}  // namespace blink

#endif  // WebRemoteFrame_h
