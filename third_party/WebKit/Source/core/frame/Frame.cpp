/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/frame/Frame.h"

#include "bindings/core/v8/WindowProxyManager.h"
#include "core/dom/DocumentType.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/api/LayoutEmbeddedContentItem.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/InstanceCounters.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/wtf/Assertions.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebRemoteFrameClient.h"

namespace blink {

using namespace HTMLNames;

Frame::~Frame() {
  InstanceCounters::DecrementCounter(InstanceCounters::kFrameCounter);
  DCHECK(!owner_);
  DCHECK_EQ(lifecycle_.GetState(), FrameLifecycle::kDetached);
}

void Frame::Trace(blink::Visitor* visitor) {
  visitor->Trace(tree_node_);
  visitor->Trace(page_);
  visitor->Trace(owner_);
  visitor->Trace(window_proxy_manager_);
  visitor->Trace(dom_window_);
  visitor->Trace(client_);
}

void Frame::Detach(FrameDetachType type) {
  DCHECK(client_);
  // By the time this method is called, the subclasses should have already
  // advanced to the Detaching state.
  DCHECK_EQ(lifecycle_.GetState(), FrameLifecycle::kDetaching);
  client_->SetOpener(nullptr);
  // After this, we must no longer talk to the client since this clears
  // its owning reference back to our owning LocalFrame.
  client_->Detached(type);
  client_ = nullptr;
  // TODO(dcheng): This currently needs to happen after calling
  // FrameClient::Detached() to make it easier for FrameClient::Detached()
  // implementations to detect provisional frames and avoid removing them from
  // the frame tree. https://crbug.com/578349.
  DisconnectOwnerElement();
  page_ = nullptr;
}

void Frame::DisconnectOwnerElement() {
  if (owner_) {
    // Ocassionally, provisional frames need to be detached, but it shouldn't
    // affect the frame tree structure. Make sure the frame owner's content
    // frame actually refers to this frame before clearing it.
    // TODO(dcheng): https://crbug.com/578349 tracks the cleanup for this once
    // it's no longer needed.
    if (owner_->ContentFrame() == this)
      owner_->ClearContentFrame();
    owner_ = nullptr;
  }
}

Page* Frame::GetPage() const {
  return page_;
}

bool Frame::IsMainFrame() const {
  return !Tree().Parent();
}

bool Frame::IsLocalRoot() const {
  if (IsRemoteFrame())
    return false;

  if (!Tree().Parent())
    return true;

  return Tree().Parent()->IsRemoteFrame();
}

HTMLFrameOwnerElement* Frame::DeprecatedLocalOwner() const {
  return owner_ && owner_->IsLocal() ? ToHTMLFrameOwnerElement(owner_)
                                     : nullptr;
}

static ChromeClient& GetEmptyChromeClient() {
  DEFINE_STATIC_LOCAL(EmptyChromeClient, client, (EmptyChromeClient::Create()));
  return client;
}

ChromeClient& Frame::GetChromeClient() const {
  if (Page* page = this->GetPage())
    return page->GetChromeClient();
  return GetEmptyChromeClient();
}

Frame* Frame::FindUnsafeParentScrollPropagationBoundary() {
  Frame* current_frame = this;
  Frame* ancestor_frame = Tree().Parent();

  while (ancestor_frame) {
    if (!ancestor_frame->GetSecurityContext()->GetSecurityOrigin()->CanAccess(
            GetSecurityContext()->GetSecurityOrigin()))
      return current_frame;
    current_frame = ancestor_frame;
    ancestor_frame = ancestor_frame->Tree().Parent();
  }
  return nullptr;
}

LayoutEmbeddedContent* Frame::OwnerLayoutObject() const {
  if (!DeprecatedLocalOwner())
    return nullptr;
  return DeprecatedLocalOwner()->GetLayoutEmbeddedContent();
}

LayoutEmbeddedContentItem Frame::OwnerLayoutItem() const {
  return LayoutEmbeddedContentItem(OwnerLayoutObject());
}

Settings* Frame::GetSettings() const {
  if (GetPage())
    return &GetPage()->GetSettings();
  return nullptr;
}

WindowProxy* Frame::GetWindowProxy(DOMWrapperWorld& world) {
  return window_proxy_manager_->GetWindowProxy(world);
}

void Frame::DidChangeVisibilityState() {
  HeapVector<Member<Frame>> child_frames;
  for (Frame* child = Tree().FirstChild(); child;
       child = child->Tree().NextSibling())
    child_frames.push_back(child);
  for (size_t i = 0; i < child_frames.size(); ++i)
    child_frames[i]->DidChangeVisibilityState();
}

void Frame::UpdateUserActivationInFrameTree() {
  user_activation_state_.Activate();
  if (Frame* parent = Tree().Parent())
    parent->UpdateUserActivationInFrameTree();
}

void Frame::NotifyUserActivation() {
  bool had_gesture = HasBeenActivated();
  if (RuntimeEnabledFeatures::UserActivationV2Enabled() || !had_gesture)
    UpdateUserActivationInFrameTree();
  if (IsLocalFrame())
    ToLocalFrame(this)->Client()->SetHasReceivedUserGesture(had_gesture);
}

// static
std::unique_ptr<UserGestureIndicator> Frame::NotifyUserActivation(
    Frame* frame,
    UserGestureToken::Status status) {
  if (frame)
    frame->NotifyUserActivation();
  return WTF::MakeUnique<UserGestureIndicator>(status);
}

// static
bool Frame::HasTransientUserActivation(Frame* frame, bool checkIfMainThread) {
  if (RuntimeEnabledFeatures::UserActivationV2Enabled()) {
    return frame ? frame->HasTransientUserActivation() : false;
  }

  return checkIfMainThread
             ? UserGestureIndicator::ProcessingUserGestureThreadSafe()
             : UserGestureIndicator::ProcessingUserGesture();
}

// static
bool Frame::ConsumeTransientUserActivation(Frame* frame,
                                           bool checkIfMainThread) {
  if (RuntimeEnabledFeatures::UserActivationV2Enabled()) {
    // TODO(mustaq): During our first phase of experiments, we will see if
    // consumption of user activation is really necessary or not.  If it turns
    // out to be unavoidable, we will replace the following call with
    // |ConsumeTransientUserActivation()|.
    return frame ? frame->HasTransientUserActivation() : false;
  }

  return checkIfMainThread
             ? UserGestureIndicator::ConsumeUserGestureThreadSafe()
             : UserGestureIndicator::ConsumeUserGesture();
}

bool Frame::IsFeatureEnabled(WebFeaturePolicyFeature feature) const {
  WebFeaturePolicy* feature_policy = GetSecurityContext()->GetFeaturePolicy();
  // The policy should always be initialized before checking it to ensure we
  // properly inherit the parent policy.
  DCHECK(feature_policy);

  // Otherwise, check policy.
  return feature_policy->IsFeatureEnabled(feature);
}

void Frame::SetOwner(FrameOwner* owner) {
  owner_ = owner;
  UpdateInertIfPossible();
}

void Frame::UpdateInertIfPossible() {
  if (owner_ && owner_->IsLocal()) {
    ToHTMLFrameOwnerElement(owner_)->UpdateDistribution();
    if (ToHTMLFrameOwnerElement(owner_)->IsInert())
      SetIsInert(true);
  }
}

Frame::Frame(FrameClient* client,
             Page& page,
             FrameOwner* owner,
             WindowProxyManager* window_proxy_manager)
    : tree_node_(this),
      page_(&page),
      owner_(owner),
      client_(client),
      window_proxy_manager_(window_proxy_manager),
      is_loading_(false) {
  InstanceCounters::IncrementCounter(InstanceCounters::kFrameCounter);

  if (owner_)
    owner_->SetContentFrame(*this);
  else
    page_->SetMainFrame(this);
}

STATIC_ASSERT_ENUM(FrameDetachType::kRemove,
                   WebFrameClient::DetachType::kRemove);
STATIC_ASSERT_ENUM(FrameDetachType::kSwap, WebFrameClient::DetachType::kSwap);
STATIC_ASSERT_ENUM(FrameDetachType::kRemove,
                   WebRemoteFrameClient::DetachType::kRemove);
STATIC_ASSERT_ENUM(FrameDetachType::kSwap,
                   WebRemoteFrameClient::DetachType::kSwap);

}  // namespace blink
