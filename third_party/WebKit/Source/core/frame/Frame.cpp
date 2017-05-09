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
#include "core/events/Event.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutPartItem.h"
#include "core/loader/EmptyClients.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/probe/CoreProbes.h"
#include "platform/Histogram.h"
#include "platform/InstanceCounters.h"
#include "platform/UserGestureIndicator.h"
#include "platform/feature_policy/FeaturePolicy.h"
#include "platform/loader/fetch/ResourceError.h"

namespace blink {

using namespace HTMLNames;

Frame::~Frame() {
  InstanceCounters::DecrementCounter(InstanceCounters::kFrameCounter);
  DCHECK(!owner_);
  DCHECK_EQ(lifecycle_.GetState(), FrameLifecycle::kDetached);
}

DEFINE_TRACE(Frame) {
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
  client_->SetOpener(0);
  DisconnectOwnerElement();
  // After this, we must no longer talk to the client since this clears
  // its owning reference back to our owning LocalFrame.
  client_->Detached(type);
  client_ = nullptr;
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

Frame* Frame::FindFrameForNavigation(const AtomicString& name,
                                     Frame& active_frame) {
  Frame* frame = Tree().Find(name);
  if (!frame || !active_frame.CanNavigate(*frame))
    return nullptr;
  return frame;
}

static bool CanAccessAncestor(const SecurityOrigin& active_security_origin,
                              const Frame* target_frame) {
  // targetFrame can be 0 when we're trying to navigate a top-level frame
  // that has a 0 opener.
  if (!target_frame)
    return false;

  const bool is_local_active_origin = active_security_origin.IsLocal();
  for (const Frame* ancestor_frame = target_frame; ancestor_frame;
       ancestor_frame = ancestor_frame->Tree().Parent()) {
    const SecurityOrigin* ancestor_security_origin =
        ancestor_frame->GetSecurityContext()->GetSecurityOrigin();
    if (active_security_origin.CanAccess(ancestor_security_origin))
      return true;

    // Allow file URL descendant navigation even when
    // allowFileAccessFromFileURLs is false.
    // FIXME: It's a bit strange to special-case local origins here. Should we
    // be doing something more general instead?
    if (is_local_active_origin && ancestor_security_origin->IsLocal())
      return true;
  }

  return false;
}

bool Frame::CanNavigate(const Frame& target_frame) {
  String error_reason;
  const bool is_allowed_navigation =
      CanNavigateWithoutFramebusting(target_frame, error_reason);
  const bool sandboxed =
      GetSecurityContext()->GetSandboxFlags() != kSandboxNone;
  const bool has_user_gesture =
      IsLocalFrame() ? ToLocalFrame(this)->HasReceivedUserGesture() : false;

  // Top navigation in sandbox with or w/o 'allow-top-navigation'.
  if (target_frame != this && sandboxed && target_frame == Tree().Top()) {
    UseCounter::Count(&target_frame, UseCounter::kTopNavInSandbox);
    if (!has_user_gesture) {
      UseCounter::Count(&target_frame,
                        UseCounter::kTopNavInSandboxWithoutGesture);
    }
  }

  // Top navigation w/o sandbox or in sandbox with 'allow-top-navigation'.
  if (target_frame != this &&
      !GetSecurityContext()->IsSandboxed(kSandboxTopNavigation) &&
      target_frame == Tree().Top()) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, framebust_histogram,
                        ("WebCore.Framebust", 4));
    const unsigned kUserGestureBit = 0x1;
    const unsigned kAllowedBit = 0x2;
    unsigned framebust_params = 0;
    UseCounter::Count(&target_frame, UseCounter::kTopNavigationFromSubFrame);

    if (has_user_gesture)
      framebust_params |= kUserGestureBit;
    if (sandboxed) {  // Sandboxed with 'allow-top-navigation'.
      UseCounter::Count(&target_frame, UseCounter::kTopNavInSandboxWithPerm);
      if (!has_user_gesture) {
        UseCounter::Count(&target_frame,
                          UseCounter::kTopNavInSandboxWithPermButNoGesture);
      }
    }

    if (is_allowed_navigation)
      framebust_params |= kAllowedBit;
    framebust_histogram.Count(framebust_params);
    if (has_user_gesture || is_allowed_navigation)
      return true;
    // Frame-busting used to be generally allowed in most situations, but may
    // now blocked if the document initiating the navigation has never received
    // a user gesture.
    if (!RuntimeEnabledFeatures::
            framebustingNeedsSameOriginOrUserGestureEnabled()) {
      String target_frame_description =
          target_frame.IsLocalFrame() ? "with URL '" +
                                            ToLocalFrame(target_frame)
                                                .GetDocument()
                                                ->Url()
                                                .GetString() +
                                            "'"
                                      : "with origin '" +
                                            target_frame.GetSecurityContext()
                                                ->GetSecurityOrigin()
                                                ->ToString() +
                                            "'";
      String message = "Frame with URL '" +
                       ToLocalFrame(this)->GetDocument()->Url().GetString() +
                       "' attempted to navigate its top-level window " +
                       target_frame_description +
                       ". Navigating the top-level window from a cross-origin "
                       "iframe will soon require that the iframe has received "
                       "a user gesture. See "
                       "https://www.chromestatus.com/features/"
                       "5851021045661696.";
      PrintNavigationWarning(message);
      return true;
    }
    error_reason =
        "The frame attempting navigation is targeting its top-level window, "
        "but is neither same-origin with its target nor has it received a "
        "user gesture. See "
        "https://www.chromestatus.com/features/5851021045661696.";
    PrintNavigationErrorMessage(target_frame, error_reason.Latin1().data());
    if (IsLocalFrame()) {
      ToLocalFrame(this)->GetNavigationScheduler().SchedulePageBlock(
          ToLocalFrame(this)->GetDocument(), ResourceError::ACCESS_DENIED);
    }
    return false;
  }
  if (!is_allowed_navigation && !error_reason.IsNull())
    PrintNavigationErrorMessage(target_frame, error_reason.Latin1().data());
  return is_allowed_navigation;
}

bool Frame::CanNavigateWithoutFramebusting(const Frame& target_frame,
                                           String& reason) {
  if (&target_frame == this)
    return true;

  if (GetSecurityContext()->IsSandboxed(kSandboxNavigation)) {
    if (!target_frame.Tree().IsDescendantOf(this) &&
        !target_frame.IsMainFrame()) {
      reason =
          "The frame attempting navigation is sandboxed, and is therefore "
          "disallowed from navigating its ancestors.";
      return false;
    }

    // Sandboxed frames can also navigate popups, if the
    // 'allow-sandbox-escape-via-popup' flag is specified, or if
    // 'allow-popups' flag is specified, or if the
    if (target_frame.IsMainFrame() && target_frame != Tree().Top() &&
        GetSecurityContext()->IsSandboxed(
            kSandboxPropagatesToAuxiliaryBrowsingContexts) &&
        (GetSecurityContext()->IsSandboxed(kSandboxPopups) ||
         target_frame.Client()->Opener() != this)) {
      reason =
          "The frame attempting navigation is sandboxed and is trying "
          "to navigate a popup, but is not the popup's opener and is not "
          "set to propagate sandboxing to popups.";
      return false;
    }

    // Top navigation is forbidden unless opted-in. allow-top-navigation or
    // allow-top-navigation-by-user-activation will also skips origin checks.
    if (target_frame == Tree().Top()) {
      if (GetSecurityContext()->IsSandboxed(kSandboxTopNavigation) &&
          GetSecurityContext()->IsSandboxed(
              kSandboxTopNavigationByUserActivation)) {
        reason =
            "The frame attempting navigation of the top-level window is "
            "sandboxed, but the flag of 'allow-top-navigation' or "
            "'allow-top-navigation-by-user-activation' is not set.";
        return false;
      }
      if (GetSecurityContext()->IsSandboxed(kSandboxTopNavigation) &&
          !GetSecurityContext()->IsSandboxed(
              kSandboxTopNavigationByUserActivation) &&
          !UserGestureIndicator::ProcessingUserGesture()) {
        // With only 'allow-top-navigation-by-user-activation' (but not
        // 'allow-top-navigation'), top navigation requires a user gesture.
        reason =
            "The frame attempting navigation of the top-level window is "
            "sandboxed with the 'allow-top-navigation-by-user-activation' "
            "flag, but has no user activation (aka gesture). See "
            "https://www.chromestatus.com/feature/5629582019395584.";
        return false;
      }
      return true;
    }
  }

  DCHECK(GetSecurityContext()->GetSecurityOrigin());
  SecurityOrigin& origin = *GetSecurityContext()->GetSecurityOrigin();

  // This is the normal case. A document can navigate its decendant frames,
  // or, more generally, a document can navigate a frame if the document is
  // in the same origin as any of that frame's ancestors (in the frame
  // hierarchy).
  //
  // See http://www.adambarth.com/papers/2008/barth-jackson-mitchell.pdf for
  // historical information about this security check.
  if (CanAccessAncestor(origin, &target_frame))
    return true;

  // Top-level frames are easier to navigate than other frames because they
  // display their URLs in the address bar (in most browsers). However, there
  // are still some restrictions on navigation to avoid nuisance attacks.
  // Specifically, a document can navigate a top-level frame if that frame
  // opened the document or if the document is the same-origin with any of
  // the top-level frame's opener's ancestors (in the frame hierarchy).
  //
  // In both of these cases, the document performing the navigation is in
  // some way related to the frame being navigate (e.g., by the "opener"
  // and/or "parent" relation). Requiring some sort of relation prevents a
  // document from navigating arbitrary, unrelated top-level frames.
  if (!target_frame.Tree().Parent()) {
    if (target_frame == Client()->Opener())
      return true;
    if (CanAccessAncestor(origin, target_frame.Client()->Opener()))
      return true;
  }

  reason =
      "The frame attempting navigation is neither same-origin with the target, "
      "nor is it the target's parent or opener.";
  return false;
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

LayoutPart* Frame::OwnerLayoutObject() const {
  if (!DeprecatedLocalOwner())
    return nullptr;
  LayoutObject* object = DeprecatedLocalOwner()->GetLayoutObject();
  if (!object)
    return nullptr;
  // FIXME: If <object> is ever fixed to disassociate itself from frames
  // that it has started but canceled, then this can turn into an ASSERT
  // since ownerElement() would be 0 when the load is canceled.
  // https://bugs.webkit.org/show_bug.cgi?id=18585
  if (!object->IsLayoutPart())
    return nullptr;
  return ToLayoutPart(object);
}

LayoutPartItem Frame::OwnerLayoutItem() const {
  return LayoutPartItem(OwnerLayoutObject());
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

void Frame::SetDocumentHasReceivedUserGesture() {
  has_received_user_gesture_ = true;
  if (Frame* parent = Tree().Parent())
    parent->SetDocumentHasReceivedUserGesture();
}

bool Frame::IsFeatureEnabled(WebFeaturePolicyFeature feature) const {
  WebFeaturePolicy* feature_policy = GetSecurityContext()->GetFeaturePolicy();
  // The policy should always be initialized before checking it to ensure we
  // properly inherit the parent policy.
  DCHECK(feature_policy);

  // Otherwise, check policy.
  return feature_policy->IsFeatureEnabled(feature);
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

}  // namespace blink
