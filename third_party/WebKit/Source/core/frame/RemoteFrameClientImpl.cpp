// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteFrameClientImpl.h"

#include <memory>
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/WebInputEventConversion.h"
#include "core/events/WheelEvent.h"
#include "core/exported/WebRemoteFrameImpl.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/geometry/IntRect.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/web/WebRemoteFrameClient.h"

namespace blink {

namespace {

// Convenience helper for frame tree helpers in FrameClient to reduce the amount
// of null-checking boilerplate code. Since the frame tree is maintained in the
// web/ layer, the frame tree helpers often have to deal with null WebFrames:
// for example, a frame with no parent will return null for WebFrame::parent().
// TODO(dcheng): Remove duplication between LocalFrameClientImpl and
// RemoteFrameClientImpl somehow...
Frame* ToCoreFrame(WebFrame* frame) {
  return frame ? WebFrame::ToCoreFrame(*frame) : nullptr;
}

}  // namespace

RemoteFrameClientImpl::RemoteFrameClientImpl(WebRemoteFrameImpl* web_frame)
    : web_frame_(web_frame) {}

RemoteFrameClientImpl* RemoteFrameClientImpl::Create(
    WebRemoteFrameImpl* web_frame) {
  return new RemoteFrameClientImpl(web_frame);
}

void RemoteFrameClientImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(web_frame_);
  RemoteFrameClient::Trace(visitor);
}

bool RemoteFrameClientImpl::InShadowTree() const {
  return web_frame_->InShadowTree();
}

void RemoteFrameClientImpl::Detached(FrameDetachType type) {
  // Alert the client that the frame is being detached.
  WebRemoteFrameClient* client = web_frame_->Client();
  if (!client)
    return;

  client->FrameDetached(static_cast<WebRemoteFrameClient::DetachType>(type));

  if (type == FrameDetachType::kRemove)
    web_frame_->DetachFromParent();

  // Clear our reference to RemoteFrame at the very end, in case the client
  // refers to it.
  web_frame_->SetCoreFrame(nullptr);
}

Frame* RemoteFrameClientImpl::Opener() const {
  return ToCoreFrame(web_frame_->Opener());
}

void RemoteFrameClientImpl::SetOpener(Frame* opener) {
  WebFrame* opener_frame = WebFrame::FromFrame(opener);
  if (web_frame_->Client() && web_frame_->Opener() != opener_frame)
    web_frame_->Client()->DidChangeOpener(opener_frame);
  web_frame_->SetOpener(opener_frame);
}

Frame* RemoteFrameClientImpl::Parent() const {
  return ToCoreFrame(web_frame_->Parent());
}

Frame* RemoteFrameClientImpl::Top() const {
  return ToCoreFrame(web_frame_->Top());
}

Frame* RemoteFrameClientImpl::NextSibling() const {
  return ToCoreFrame(web_frame_->NextSibling());
}

Frame* RemoteFrameClientImpl::FirstChild() const {
  return ToCoreFrame(web_frame_->FirstChild());
}

void RemoteFrameClientImpl::FrameFocused() const {
  if (web_frame_->Client())
    web_frame_->Client()->FrameFocused();
}

base::UnguessableToken RemoteFrameClientImpl::GetDevToolsFrameToken() const {
  if (web_frame_->Client()) {
    return web_frame_->Client()->GetDevToolsFrameToken();
  }
  return base::UnguessableToken::Create();
}

void RemoteFrameClientImpl::Navigate(const ResourceRequest& request,
                                     bool should_replace_current_entry) {
  if (web_frame_->Client()) {
    web_frame_->Client()->Navigate(WrappedResourceRequest(request),
                                   should_replace_current_entry);
  }
}

void RemoteFrameClientImpl::Reload(
    FrameLoadType load_type,
    ClientRedirectPolicy client_redirect_policy) {
  DCHECK(IsReloadLoadType(load_type));
  if (web_frame_->Client()) {
    web_frame_->Client()->Reload(static_cast<WebFrameLoadType>(load_type),
                                 client_redirect_policy);
  }
}

unsigned RemoteFrameClientImpl::BackForwardLength() {
  // TODO(creis,japhet): This method should return the real value for the
  // session history length. For now, return static value for the initial
  // navigation and the subsequent one moving the frame out-of-process.
  // See https://crbug.com/501116.
  return 2;
}

void RemoteFrameClientImpl::CheckCompleted() {
  web_frame_->Client()->CheckCompleted();
}

void RemoteFrameClientImpl::ForwardPostMessage(
    MessageEvent* event,
    scoped_refptr<const SecurityOrigin> target,
    LocalFrame* source_frame) const {
  // Restrict the user gesture to be forwarded cross-process at most once. This
  // helps avoid unbounded usage of the same user gesture by issuing multiple
  // postMessages to OOPIFs from this process.  A complementary restriction on
  // the receiver side prevents unbounded chaining of user gestures across
  // processes.
  bool has_user_gesture = UserGestureIndicator::ProcessingUserGesture() &&
                          !UserGestureIndicator::WasForwardedCrossProcess();
  if (has_user_gesture)
    UserGestureIndicator::SetWasForwardedCrossProcess();

  if (web_frame_->Client()) {
    web_frame_->Client()->ForwardPostMessage(
        WebLocalFrameImpl::FromFrame(source_frame), web_frame_,
        WebSecurityOrigin(std::move(target)), WebDOMMessageEvent(event),
        has_user_gesture);
  }
}

void RemoteFrameClientImpl::FrameRectsChanged(
    const IntRect& local_frame_rect,
    const IntRect& screen_space_rect) {
  web_frame_->Client()->FrameRectsChanged(local_frame_rect, screen_space_rect);
}

void RemoteFrameClientImpl::UpdateRemoteViewportIntersection(
    const IntRect& viewport_intersection) {
  web_frame_->Client()->UpdateRemoteViewportIntersection(viewport_intersection);
}

void RemoteFrameClientImpl::AdvanceFocus(WebFocusType type,
                                         LocalFrame* source) {
  web_frame_->Client()->AdvanceFocus(type,
                                     WebLocalFrameImpl::FromFrame(source));
}

void RemoteFrameClientImpl::VisibilityChanged(bool visible) {
  web_frame_->Client()->VisibilityChanged(visible);
}

void RemoteFrameClientImpl::SetIsInert(bool inert) {
  web_frame_->Client()->SetIsInert(inert);
}

void RemoteFrameClientImpl::UpdateRenderThrottlingStatus(
    bool is_throttled,
    bool subtree_throttled) {
  web_frame_->Client()->UpdateRenderThrottlingStatus(is_throttled,
                                                     subtree_throttled);
}

uint32_t RemoteFrameClientImpl::Print(const IntRect& rect,
                                      WebCanvas* canvas) const {
  return web_frame_->Client()->Print(rect, canvas);
}

}  // namespace blink
