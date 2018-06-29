// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/remote_frame.h"

#include "third_party/blink/renderer/bindings/core/v8/window_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_dom_window.h"
#include "third_party/blink/renderer/core/frame/remote_frame_client.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/plugins/plugin_script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

inline RemoteFrame::RemoteFrame(RemoteFrameClient* client,
                                Page& page,
                                FrameOwner* owner)
    : Frame(client, page, owner, RemoteWindowProxyManager::Create(*this)),
      security_context_(RemoteSecurityContext::Create()) {
  dom_window_ = RemoteDOMWindow::Create(*this);
  UpdateInertIfPossible();
  UpdateInheritedEffectiveTouchActionIfPossible();
}

RemoteFrame* RemoteFrame::Create(RemoteFrameClient* client,
                                 Page& page,
                                 FrameOwner* owner) {
  RemoteFrame* frame = new RemoteFrame(client, page, owner);
  PageScheduler* page_scheduler = page.GetPageScheduler();
  if (frame->IsMainFrame() && page_scheduler)
    page_scheduler->SetIsMainFrameLocal(false);
  return frame;
}

RemoteFrame::~RemoteFrame() {
  DCHECK(!view_);
}

void RemoteFrame::Trace(blink::Visitor* visitor) {
  visitor->Trace(view_);
  visitor->Trace(security_context_);
  Frame::Trace(visitor);
}

void RemoteFrame::ScheduleNavigation(Document& origin_document,
                                     const KURL& url,
                                     bool replace_current_item,
                                     UserGestureStatus user_gesture_status) {
  FrameLoadRequest frame_request(&origin_document, ResourceRequest(url));
  frame_request.SetReplacesCurrentItem(replace_current_item);
  frame_request.GetResourceRequest().SetHasUserGesture(
      user_gesture_status == UserGestureStatus::kActive);
  frame_request.GetResourceRequest().SetFrameType(
      IsMainFrame() ? network::mojom::RequestContextFrameType::kTopLevel
                    : network::mojom::RequestContextFrameType::kNested);
  Navigate(frame_request);
}

void RemoteFrame::Navigate(const FrameLoadRequest& passed_request) {
  FrameLoadRequest frame_request(passed_request);

  // The process where this frame actually lives won't have sufficient
  // information to determine correct referrer and upgrade the url, since it
  // won't have access to the originDocument. Do it now.
  FrameLoader::SetReferrerForFrameRequest(frame_request);
  FrameLoader::UpgradeInsecureRequest(frame_request.GetResourceRequest(),
                                      frame_request.OriginDocument());

  Client()->Navigate(frame_request.GetResourceRequest(),
                     frame_request.ReplacesCurrentItem(),
                     frame_request.GetBlobURLToken());
}

void RemoteFrame::Reload(WebFrameLoadType frame_load_type,
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
  GetWindowProxyManager()->ClearForClose();
  SetView(nullptr);
  // ... the RemoteDOMWindow will need to be informed of detachment,
  // as otherwise it will keep a strong reference back to this RemoteFrame.
  // That combined with wrappers (owned and kept alive by RemoteFrame) keeping
  // persistent strong references to RemoteDOMWindow will prevent the GCing
  // of all these objects. Break the cycle by notifying of detachment.
  ToRemoteDOMWindow(dom_window_)->FrameDetached();
  if (cc_layer_)
    SetCcLayer(nullptr, false);
  Frame::Detach(type);
}

bool RemoteFrame::PrepareForCommit() {
  DetachChildren();
  return !!GetPage();
}

void RemoteFrame::CheckCompleted() {
  // Notify the client so that the corresponding LocalFrame can do the check.
  Client()->CheckCompleted();
}

RemoteSecurityContext* RemoteFrame::GetSecurityContext() const {
  return security_context_.Get();
}

bool RemoteFrame::ShouldClose() {
  // TODO(nasko): Implement running the beforeunload handler in the actual
  // LocalFrame running in a different process and getting back a real result.
  return true;
}

void RemoteFrame::DidFreeze() {
  DCHECK(RuntimeEnabledFeatures::PageLifecycleEnabled());
  // TODO(fmeawad): Add support for remote frames.
}

void RemoteFrame::DidResume() {
  DCHECK(RuntimeEnabledFeatures::PageLifecycleEnabled());
  // TODO(fmeawad): Add support for remote frames.
}

void RemoteFrame::SetIsInert(bool inert) {
  if (inert != is_inert_)
    Client()->SetIsInert(inert);
  is_inert_ = inert;
}

void RemoteFrame::SetInheritedEffectiveTouchAction(TouchAction touch_action) {
  if (inherited_effective_touch_action_ != touch_action)
    Client()->SetInheritedEffectiveTouchAction(touch_action);
  inherited_effective_touch_action_ = touch_action;
}

bool RemoteFrame::BubbleLogicalScrollFromChildFrame(
    ScrollDirection direction,
    ScrollGranularity granularity,
    Frame* child) {
  DCHECK(child->IsLocalFrame());
  DCHECK(child->Client());
  ToLocalFrame(child)->Client()->BubbleLogicalScrollInParentFrame(direction,
                                                                  granularity);
  return false;
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

  if (OwnerLayoutObject())
    DeprecatedLocalOwner()->SetEmbeddedContentView(view_);
}

RemoteFrameClient* RemoteFrame::Client() const {
  return static_cast<RemoteFrameClient*>(Frame::Client());
}

void RemoteFrame::SetCcLayer(cc::Layer* cc_layer,
                             bool prevent_contents_opaque_changes) {
  if (cc_layer_)
    GraphicsLayer::UnregisterContentsLayer(cc_layer_);
  cc_layer_ = cc_layer;
  prevent_contents_opaque_changes_ = prevent_contents_opaque_changes;
  if (cc_layer_)
    GraphicsLayer::RegisterContentsLayer(cc_layer_);

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
