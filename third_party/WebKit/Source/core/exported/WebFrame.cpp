// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebFrame.h"

#include <algorithm>
#include "bindings/core/v8/WindowProxyManager.h"
#include "core/HTMLNames.h"
#include "core/dom/IncrementLoadEventDelayCount.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/exported/WebRemoteFrameImpl.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/OpenedFrameTracker.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameOwner.h"
#include "core/frame/WebLocalFrameImpl.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/page/Page.h"
#include "platform/heap/Handle.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/web/WebElement.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebSandboxFlags.h"

namespace blink {

bool WebFrame::Swap(WebFrame* frame) {
  using std::swap;
  Frame* old_frame = ToCoreFrame(*this);
  if (!old_frame->IsAttached())
    return false;

  // Unload the current Document in this frame: this calls unload handlers,
  // detaches child frames, etc. Since this runs script, make sure this frame
  // wasn't detached before continuing with the swap.
  // FIXME: There is no unit test for this condition, so one needs to be
  // written.
  if (!old_frame->PrepareForCommit())
    return false;

  // If there is a local parent, it might incorrectly declare itself complete
  // during the detach phase of this swap. Suppress its completion until swap is
  // over, at which point its completion will be correctly dependent on its
  // newly swapped-in child.
  std::unique_ptr<IncrementLoadEventDelayCount> delay_parent_load =
      parent_ && parent_->IsWebLocalFrame()
          ? IncrementLoadEventDelayCount::Create(
                *ToWebLocalFrameImpl(parent_)->GetFrame()->GetDocument())
          : nullptr;

  if (parent_) {
    if (parent_->first_child_ == this)
      parent_->first_child_ = frame;
    if (parent_->last_child_ == this)
      parent_->last_child_ = frame;
    // FIXME: This is due to the fact that the |frame| may be a provisional
    // local frame, because we don't know if the navigation will result in
    // an actual page or something else, like a download. The PlzNavigate
    // project will remove the need for provisional local frames.
    frame->parent_ = parent_;
  }

  if (previous_sibling_) {
    previous_sibling_->next_sibling_ = frame;
    swap(previous_sibling_, frame->previous_sibling_);
  }
  if (next_sibling_) {
    next_sibling_->previous_sibling_ = frame;
    swap(next_sibling_, frame->next_sibling_);
  }

  if (opener_) {
    frame->SetOpener(opener_);
    SetOpener(nullptr);
  }
  opened_frame_tracker_->TransferTo(frame);

  Page* page = old_frame->GetPage();
  AtomicString name = old_frame->Tree().GetName();
  FrameOwner* owner = old_frame->Owner();

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  WindowProxyManager::GlobalProxyVector global_proxies;
  old_frame->GetWindowProxyManager()->ClearForSwap();
  old_frame->GetWindowProxyManager()->ReleaseGlobalProxies(global_proxies);

  // Although the Document in this frame is now unloaded, many resources
  // associated with the frame itself have not yet been freed yet.
  old_frame->Detach(FrameDetachType::kSwap);

  // Clone the state of the current Frame into the one being swapped in.
  // FIXME: This is a bit clunky; this results in pointless decrements and
  // increments of connected subframes.
  if (frame->IsWebLocalFrame()) {
    // TODO(dcheng): in an ideal world, both branches would just use
    // WebFrame's initializeCoreFrame() helper. However, Blink
    // currently requires a 'provisional' local frame to serve as a
    // placeholder for loading state when swapping to a local frame.
    // In this case, the core LocalFrame is already initialized, so just
    // update a bit of state.
    LocalFrame& local_frame = *ToWebLocalFrameImpl(frame)->GetFrame();
    DCHECK_EQ(owner, local_frame.Owner());
    if (owner) {
      owner->SetContentFrame(local_frame);
      if (owner->IsLocal()) {
        ToHTMLFrameOwnerElement(owner)->SetEmbeddedContentView(
            local_frame.View());
      }
    } else {
      local_frame.GetPage()->SetMainFrame(&local_frame);
      // This trace event is needed to detect the main frame of the
      // renderer in telemetry metrics. See crbug.com/692112#c11.
      TRACE_EVENT_INSTANT1("loading", "markAsMainFrame",
                           TRACE_EVENT_SCOPE_THREAD, "frame", &local_frame);
    }
  } else {
    ToWebRemoteFrameImpl(frame)->InitializeCoreFrame(*page, owner, name);
  }

  if (parent_ && old_frame->HasReceivedUserGesture())
    ToCoreFrame(*frame)->UpdateUserActivationInFrameTree();

  ToCoreFrame(*frame)->GetWindowProxyManager()->SetGlobalProxies(
      global_proxies);

  parent_ = nullptr;

  return true;
}

void WebFrame::Detach() {
  ToCoreFrame(*this)->Detach(FrameDetachType::kRemove);
}

WebSecurityOrigin WebFrame::GetSecurityOrigin() const {
  return WebSecurityOrigin(
      ToCoreFrame(*this)->GetSecurityContext()->GetSecurityOrigin());
}

void WebFrame::SetFrameOwnerPolicy(
    WebSandboxFlags flags,
    const blink::WebParsedFeaturePolicy& container_policy) {
  // At the moment, this is only used to replicate sandbox flags and container
  // policy for frames with a remote owner.
  RemoteFrameOwner* owner = ToRemoteFrameOwner(ToCoreFrame(*this)->Owner());
  DCHECK(owner);
  owner->SetSandboxFlags(static_cast<SandboxFlags>(flags));
  owner->SetContainerPolicy(container_policy);
}

WebInsecureRequestPolicy WebFrame::GetInsecureRequestPolicy() const {
  return ToCoreFrame(*this)->GetSecurityContext()->GetInsecureRequestPolicy();
}

void WebFrame::SetFrameOwnerProperties(
    const WebFrameOwnerProperties& properties) {
  // At the moment, this is only used to replicate frame owner properties
  // for frames with a remote owner.
  RemoteFrameOwner* owner = ToRemoteFrameOwner(ToCoreFrame(*this)->Owner());
  DCHECK(owner);

  Frame* frame = ToCoreFrame(*this);
  DCHECK(frame);

  if (frame->IsLocalFrame()) {
    ToLocalFrame(frame)->GetDocument()->WillChangeFrameOwnerProperties(
        properties.margin_width, properties.margin_height,
        static_cast<ScrollbarMode>(properties.scrolling_mode),
        properties.is_display_none);
  }

  owner->SetBrowsingContextContainerName(properties.name);
  owner->SetScrollingMode(properties.scrolling_mode);
  owner->SetMarginWidth(properties.margin_width);
  owner->SetMarginHeight(properties.margin_height);
  owner->SetAllowFullscreen(properties.allow_fullscreen);
  owner->SetAllowPaymentRequest(properties.allow_payment_request);
  owner->SetIsDisplayNone(properties.is_display_none);
  owner->SetCsp(properties.required_csp);
}

void WebFrame::Collapse(bool collapsed) {
  FrameOwner* owner = ToCoreFrame(*this)->Owner();
  DCHECK(owner->IsLocal());
  ToHTMLFrameOwnerElement(owner)->SetCollapsed(collapsed);
}

WebFrame* WebFrame::Opener() const {
  return opener_;
}

void WebFrame::SetOpener(WebFrame* opener) {
  if (opener_)
    opener_->opened_frame_tracker_->Remove(this);
  if (opener)
    opener->opened_frame_tracker_->Add(this);
  opener_ = opener;
}

void WebFrame::ClearOpener() {
  SetOpener(nullptr);
}

void WebFrame::InsertAfter(WebFrame* new_child, WebFrame* previous_sibling) {
  new_child->parent_ = this;

  WebFrame* next;
  if (!previous_sibling) {
    // Insert at the beginning if no previous sibling is specified.
    next = first_child_;
    first_child_ = new_child;
  } else {
    DCHECK_EQ(previous_sibling->parent_, this);
    next = previous_sibling->next_sibling_;
    previous_sibling->next_sibling_ = new_child;
    new_child->previous_sibling_ = previous_sibling;
  }

  if (next) {
    new_child->next_sibling_ = next;
    next->previous_sibling_ = new_child;
  } else {
    last_child_ = new_child;
  }

  ToCoreFrame(*this)->Tree().InvalidateScopedChildCount();
  ToCoreFrame(*this)->GetPage()->IncrementSubframeCount();
}

void WebFrame::AppendChild(WebFrame* child) {
  // TODO(dcheng): Original code asserts that the frames have the same Page.
  // We should add an equivalent check... figure out what.
  InsertAfter(child, last_child_);
}

void WebFrame::RemoveChild(WebFrame* child) {
  child->parent_ = 0;

  if (first_child_ == child)
    first_child_ = child->next_sibling_;
  else
    child->previous_sibling_->next_sibling_ = child->next_sibling_;

  if (last_child_ == child)
    last_child_ = child->previous_sibling_;
  else
    child->next_sibling_->previous_sibling_ = child->previous_sibling_;

  child->previous_sibling_ = child->next_sibling_ = 0;

  ToCoreFrame(*this)->Tree().InvalidateScopedChildCount();
  ToCoreFrame(*this)->GetPage()->DecrementSubframeCount();
}

void WebFrame::SetParent(WebFrame* parent) {
  parent_ = parent;
}

WebFrame* WebFrame::Parent() const {
  return parent_;
}

WebFrame* WebFrame::Top() const {
  WebFrame* frame = const_cast<WebFrame*>(this);
  for (WebFrame* parent = frame; parent; parent = parent->parent_)
    frame = parent;
  return frame;
}

WebFrame* WebFrame::FirstChild() const {
  return first_child_;
}

WebFrame* WebFrame::NextSibling() const {
  return next_sibling_;
}

WebFrame* WebFrame::TraverseNext() const {
  if (Frame* frame = ToCoreFrame(*this))
    return FromFrame(frame->Tree().TraverseNext());
  return nullptr;
}

WebFrame* WebFrame::FromFrameOwnerElement(const WebElement& web_element) {
  Element* element = web_element;

  if (!element->IsFrameOwnerElement())
    return nullptr;
  return FromFrame(ToHTMLFrameOwnerElement(element)->ContentFrame());
}

bool WebFrame::IsLoading() const {
  if (Frame* frame = ToCoreFrame(*this))
    return frame->IsLoading();
  return false;
}

WebFrame* WebFrame::FromFrame(Frame* frame) {
  if (!frame)
    return 0;

  if (frame->IsLocalFrame())
    return WebLocalFrameImpl::FromFrame(ToLocalFrame(*frame));
  return WebRemoteFrameImpl::FromFrame(ToRemoteFrame(*frame));
}

WebFrame::WebFrame(WebTreeScopeType scope)
    : scope_(scope),
      parent_(0),
      previous_sibling_(0),
      next_sibling_(0),
      first_child_(0),
      last_child_(0),
      opener_(0),
      opened_frame_tracker_(new OpenedFrameTracker) {}

WebFrame::~WebFrame() {
  opened_frame_tracker_.reset(0);
}

void WebFrame::TraceFrame(Visitor* visitor, WebFrame* frame) {
  if (!frame)
    return;

  if (frame->IsWebLocalFrame())
    visitor->Trace(ToWebLocalFrameImpl(frame));
  else
    visitor->Trace(ToWebRemoteFrameImpl(frame));
}

void WebFrame::TraceFrames(Visitor* visitor, WebFrame* frame) {
  DCHECK(frame);
  TraceFrame(visitor, frame->parent_);
  for (WebFrame* child = frame->FirstChild(); child;
       child = child->NextSibling())
    TraceFrame(visitor, child);
}

void WebFrame::Close() {
  opened_frame_tracker_->Dispose();
}

void WebFrame::DetachFromParent() {
  // TODO(dcheng): This should really just check if there's a parent, and call
  // RemoveChild() if so. Once provisional frames are removed, this check can be
  // simplified to just check Parent(). See https://crbug.com/578349.
  const blink::Frame* frame = ToCoreFrame(*this);
  if (frame->Owner() && frame->Owner()->ContentFrame() == frame)
    Parent()->RemoveChild(this);
}

Frame* WebFrame::ToCoreFrame(const WebFrame& frame) {
  if (frame.IsWebLocalFrame())
    return ToWebLocalFrameImpl(frame).GetFrame();
  if (frame.IsWebRemoteFrame())
    return ToWebRemoteFrameImpl(frame).GetFrame();
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
