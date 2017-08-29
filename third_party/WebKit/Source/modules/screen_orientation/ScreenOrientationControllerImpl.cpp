// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/screen_orientation/ScreenOrientationControllerImpl.h"

#include <memory>
#include <utility>
#include "core/dom/Document.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/events/Event.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "modules/screen_orientation/ScreenOrientation.h"
#include "modules/screen_orientation/ScreenOrientationDispatcher.h"
#include "platform/LayoutTestSupport.h"
#include "platform/ScopedOrientationChangeIndicator.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationClient.h"

namespace blink {

ScreenOrientationControllerImpl::~ScreenOrientationControllerImpl() = default;

void ScreenOrientationControllerImpl::ProvideTo(
    LocalFrame& frame,
    WebScreenOrientationClient* client) {
  ScreenOrientationController::ProvideTo(
      frame, new ScreenOrientationControllerImpl(frame, client));
}

ScreenOrientationControllerImpl* ScreenOrientationControllerImpl::From(
    LocalFrame& frame) {
  return static_cast<ScreenOrientationControllerImpl*>(
      ScreenOrientationController::From(frame));
}

ScreenOrientationControllerImpl::ScreenOrientationControllerImpl(
    LocalFrame& frame,
    WebScreenOrientationClient* client)
    : ScreenOrientationController(frame),
      ContextLifecycleObserver(frame.GetDocument()),
      PlatformEventController(&frame),
      client_(client),
      dispatch_event_timer_(
          TaskRunnerHelper::Get(TaskType::kMiscPlatformAPI, &frame),
          this,
          &ScreenOrientationControllerImpl::DispatchEventTimerFired) {}

// Compute the screen orientation using the orientation angle and the screen
// width / height.
WebScreenOrientationType ScreenOrientationControllerImpl::ComputeOrientation(
    const IntRect& rect,
    uint16_t rotation) {
  // Bypass orientation detection in layout tests to get consistent results.
  // FIXME: The screen dimension should be fixed when running the layout tests
  // to avoid such issues.
  if (LayoutTestSupport::IsRunningLayoutTest())
    return kWebScreenOrientationPortraitPrimary;

  bool is_tall_display = rotation % 180 ? rect.Height() < rect.Width()
                                        : rect.Height() > rect.Width();

  // https://w3c.github.io/screen-orientation/#dfn-current-orientation-angle
  // allows the UA to associate *-primary and *-secondary values at will. Blink
  // arbitrarily chooses rotation 0 to always be portrait-primary or
  // landscape-primary, and portrait-primary + 90 to be landscape-primary, which
  // together fully determine the relationship.
  switch (rotation) {
    case 0:
      return is_tall_display ? kWebScreenOrientationPortraitPrimary
                             : kWebScreenOrientationLandscapePrimary;
    case 90:
      return is_tall_display ? kWebScreenOrientationLandscapePrimary
                             : kWebScreenOrientationPortraitSecondary;
    case 180:
      return is_tall_display ? kWebScreenOrientationPortraitSecondary
                             : kWebScreenOrientationLandscapeSecondary;
    case 270:
      return is_tall_display ? kWebScreenOrientationLandscapeSecondary
                             : kWebScreenOrientationPortraitPrimary;
    default:
      NOTREACHED();
      return kWebScreenOrientationPortraitPrimary;
  }
}

void ScreenOrientationControllerImpl::UpdateOrientation() {
  DCHECK(orientation_);
  DCHECK(GetFrame());
  DCHECK(GetFrame()->GetPage());
  ChromeClient& chrome_client = GetFrame()->GetPage()->GetChromeClient();
  WebScreenInfo screen_info = chrome_client.GetScreenInfo();
  WebScreenOrientationType orientation_type = screen_info.orientation_type;
  if (orientation_type == kWebScreenOrientationUndefined) {
    // The embedder could not provide us with an orientation, deduce it
    // ourselves.
    orientation_type = ComputeOrientation(chrome_client.GetScreenInfo().rect,
                                          screen_info.orientation_angle);
  }
  DCHECK(orientation_type != kWebScreenOrientationUndefined);

  orientation_->SetType(orientation_type);
  orientation_->SetAngle(screen_info.orientation_angle);
}

bool ScreenOrientationControllerImpl::IsActive() const {
  return orientation_ && client_;
}

bool ScreenOrientationControllerImpl::IsVisible() const {
  return GetPage() && GetPage()->IsPageVisible();
}

bool ScreenOrientationControllerImpl::IsActiveAndVisible() const {
  return IsActive() && IsVisible();
}

void ScreenOrientationControllerImpl::PageVisibilityChanged() {
  NotifyDispatcher();

  if (!IsActiveAndVisible())
    return;

  DCHECK(GetFrame());
  DCHECK(GetFrame()->GetPage());

  // The orientation type and angle are tied in a way that if the angle has
  // changed, the type must have changed.
  unsigned short current_angle = GetFrame()
                                     ->GetPage()
                                     ->GetChromeClient()
                                     .GetScreenInfo()
                                     .orientation_angle;

  // FIXME: sendOrientationChangeEvent() currently send an event all the
  // children of the frame, so it should only be called on the frame on
  // top of the tree. We would need the embedder to call
  // sendOrientationChangeEvent on every WebFrame part of a WebView to be
  // able to remove this.
  if (GetFrame() == GetFrame()->LocalFrameRoot() &&
      orientation_->angle() != current_angle)
    NotifyOrientationChanged();
}

void ScreenOrientationControllerImpl::NotifyOrientationChanged() {
  if (!IsVisible() || !GetFrame())
    return;

  if (IsActive())
    UpdateOrientation();

  // Keep track of the frames that need to be notified before notifying the
  // current frame as it will prevent side effects from the change event
  // handlers.
  HeapVector<Member<LocalFrame>> child_frames;
  for (Frame* child = GetFrame()->Tree().FirstChild(); child;
       child = child->Tree().NextSibling()) {
    if (child->IsLocalFrame())
      child_frames.push_back(ToLocalFrame(child));
  }

  // Notify current orientation object.
  if (IsActive() && !dispatch_event_timer_.IsActive())
    dispatch_event_timer_.StartOneShot(0, BLINK_FROM_HERE);

  // ... and child frames, if they have a ScreenOrientationControllerImpl.
  for (size_t i = 0; i < child_frames.size(); ++i) {
    if (ScreenOrientationControllerImpl* controller =
            ScreenOrientationControllerImpl::From(*child_frames[i])) {
      controller->NotifyOrientationChanged();
    }
  }
}

void ScreenOrientationControllerImpl::SetOrientation(
    ScreenOrientation* orientation) {
  orientation_ = orientation;
  if (orientation_)
    UpdateOrientation();
  NotifyDispatcher();
}

void ScreenOrientationControllerImpl::lock(
    WebScreenOrientationLockType orientation,
    std::unique_ptr<WebLockOrientationCallback> callback) {
  // When detached, the client is no longer valid.
  if (!client_)
    return;
  client_->LockOrientation(orientation, std::move(callback));
  active_lock_ = true;
}

void ScreenOrientationControllerImpl::unlock() {
  // When detached, the client is no longer valid.
  if (!client_)
    return;
  client_->UnlockOrientation();
  active_lock_ = false;
}

bool ScreenOrientationControllerImpl::MaybeHasActiveLock() const {
  return active_lock_;
}

void ScreenOrientationControllerImpl::DispatchEventTimerFired(TimerBase*) {
  if (!orientation_)
    return;

  ScopedOrientationChangeIndicator orientation_change_indicator;
  orientation_->DispatchEvent(Event::Create(EventTypeNames::change));
}

void ScreenOrientationControllerImpl::DidUpdateData() {
  // Do nothing.
}

void ScreenOrientationControllerImpl::RegisterWithDispatcher() {
  ScreenOrientationDispatcher::Instance().AddController(this);
}

void ScreenOrientationControllerImpl::UnregisterWithDispatcher() {
  ScreenOrientationDispatcher::Instance().RemoveController(this);
}

bool ScreenOrientationControllerImpl::HasLastData() {
  return true;
}

void ScreenOrientationControllerImpl::ContextDestroyed(ExecutionContext*) {
  StopUpdating();
  client_ = nullptr;
  active_lock_ = false;
}

void ScreenOrientationControllerImpl::NotifyDispatcher() {
  if (orientation_ && GetPage()->IsPageVisible())
    StartUpdating();
  else
    StopUpdating();
}

DEFINE_TRACE(ScreenOrientationControllerImpl) {
  visitor->Trace(orientation_);
  ContextLifecycleObserver::Trace(visitor);
  Supplement<LocalFrame>::Trace(visitor);
  PlatformEventController::Trace(visitor);
}

}  // namespace blink
