// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RemoteFrame.h"

#include "bindings/core/v8/WindowProxy.h"
#include "bindings/core/v8/WindowProxyManager.h"
#include "core/dom/RemoteSecurityContext.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/RemoteDOMWindow.h"
#include "core/frame/RemoteFrameClient.h"
#include "core/frame/RemoteFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/api/LayoutEmbeddedContentItem.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/plugins/PluginScriptForbiddenScope.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/WebLayer.h"

namespace blink {

inline RemoteFrame::RemoteFrame(RemoteFrameClient* client,
                                Page& page,
                                FrameOwner* owner)
    : Frame(client, page, owner, RemoteWindowProxyManager::Create(*this)),
      security_context_(RemoteSecurityContext::Create()) {
  dom_window_ = RemoteDOMWindow::Create(*this);
  UpdateInertIfPossible();
}

RemoteFrame* RemoteFrame::Create(RemoteFrameClient* client,
                                 Page& page,
                                 FrameOwner* owner) {
  return new RemoteFrame(client, page, owner);
}

RemoteFrame::~RemoteFrame() {
  DCHECK(!view_);
}

DEFINE_TRACE(RemoteFrame) {
  visitor->Trace(view_);
  visitor->Trace(security_context_);
  Frame::Trace(visitor);
}

void RemoteFrame::Navigate(Document& origin_document,
                           const KURL& url,
                           bool replace_current_item,
                           UserGestureStatus user_gesture_status) {
  FrameLoadRequest frame_request(&origin_document, ResourceRequest(url));
  frame_request.SetReplacesCurrentItem(replace_current_item);
  frame_request.GetResourceRequest().SetHasUserGesture(
      user_gesture_status == UserGestureStatus::kActive);
  Navigate(frame_request);
}

void RemoteFrame::Navigate(const FrameLoadRequest& passed_request) {
  FrameLoadRequest frame_request(passed_request);

  // The process where this frame actually lives won't have sufficient
  // information to determine correct referrer, since it won't have access to
  // the originDocument. Set it now.
  FrameLoader::SetReferrerForFrameRequest(frame_request);

  frame_request.GetResourceRequest().SetHasUserGesture(
      UserGestureIndicator::ProcessingUserGesture());
  Client()->Navigate(frame_request.GetResourceRequest(),
                     frame_request.ReplacesCurrentItem());
}

void RemoteFrame::Reload(FrameLoadType frame_load_type,
                         ClientRedirectPolicy client_redirect_policy) {
  Client()->Reload(frame_load_type, client_redirect_policy);
}

void RemoteFrame::Detach(FrameDetachType type) {
  lifecycle_.AdvanceTo(FrameLifecycle::kDetaching);

  PluginScriptForbiddenScope forbid_plugin_destructor_scripting;
  DetachChildren();
  if (!Client())
    return;

  // Clean up the frame's view if needed. A remote frame only has a view if
  // the parent is a local frame.
  if (view_)
    view_->Dispose();
  Client()->WillBeDetached();
  GetWindowProxyManager()->ClearForClose();
  SetView(nullptr);
  // ... the RemoteDOMWindow will need to be informed of detachment,
  // as otherwise it will keep a strong reference back to this RemoteFrame.
  // That combined with wrappers (owned and kept alive by RemoteFrame) keeping
  // persistent strong references to RemoteDOMWindow will prevent the GCing
  // of all these objects. Break the cycle by notifying of detachment.
  ToRemoteDOMWindow(dom_window_)->FrameDetached();
  if (web_layer_)
    SetWebLayer(nullptr);
  Frame::Detach(type);
  lifecycle_.AdvanceTo(FrameLifecycle::kDetached);
}

bool RemoteFrame::PrepareForCommit() {
  DetachChildren();
  return !!GetPage();
}

RemoteSecurityContext* RemoteFrame::GetSecurityContext() const {
  return security_context_.Get();
}

bool RemoteFrame::ShouldClose() {
  // TODO(nasko): Implement running the beforeunload handler in the actual
  // LocalFrame running in a different process and getting back a real result.
  return true;
}

void RemoteFrame::SetIsInert(bool inert) {
  if (inert != is_inert_)
    Client()->SetIsInert(inert);
  is_inert_ = inert;
}

void RemoteFrame::SetView(RemoteFrameView* view) {
  // Oilpan: as RemoteFrameView performs no finalization actions,
  // no explicit Dispose() of it needed here. (cf. LocalFrameView::Dispose().)
  view_ = view;
}

void RemoteFrame::CreateView() {
  // If the RemoteFrame does not have a LocalFrame parent, there's no need to
  // create a EmbeddedContentView for it.
  if (!DeprecatedLocalOwner())
    return;

  DCHECK(!DeprecatedLocalOwner()->OwnedEmbeddedContentView());

  SetView(RemoteFrameView::Create(this));

  if (!OwnerLayoutItem().IsNull())
    DeprecatedLocalOwner()->SetEmbeddedContentView(view_);
}

RemoteFrameClient* RemoteFrame::Client() const {
  return static_cast<RemoteFrameClient*>(Frame::Client());
}

void RemoteFrame::SetWebLayer(WebLayer* web_layer) {
  if (web_layer_)
    GraphicsLayer::UnregisterContentsLayer(web_layer_);
  web_layer_ = web_layer;
  if (web_layer_)
    GraphicsLayer::RegisterContentsLayer(web_layer_);

  DCHECK(Owner());
  ToHTMLFrameOwnerElement(Owner())->SetNeedsCompositingUpdate();
}

void RemoteFrame::AdvanceFocus(WebFocusType type, LocalFrame* source) {
  Client()->AdvanceFocus(type, source);
}

void RemoteFrame::DetachChildren() {
  using FrameVector = HeapVector<Member<Frame>>;
  FrameVector children_to_detach;
  children_to_detach.ReserveCapacity(Tree().ChildCount());
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    children_to_detach.push_back(child);
  for (const auto& child : children_to_detach)
    child->Detach(FrameDetachType::kRemove);
}

}  // namespace blink
