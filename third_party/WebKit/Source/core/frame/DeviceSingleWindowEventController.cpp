// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/DeviceSingleWindowEventController.h"

#include "core/dom/Document.h"
#include "core/dom/events/Event.h"
#include "core/page/Page.h"

namespace blink {

DeviceSingleWindowEventController::DeviceSingleWindowEventController(
    Document& document)
    : PlatformEventController(document.GetFrame()),
      needs_checking_null_events_(true),
      document_(document) {
  document.domWindow()->RegisterEventListenerObserver(this);
}

DeviceSingleWindowEventController::~DeviceSingleWindowEventController() {}

void DeviceSingleWindowEventController::DidUpdateData() {
  DispatchDeviceEvent(LastEvent());
}

void DeviceSingleWindowEventController::DispatchDeviceEvent(Event* event) {
  if (!GetDocument().domWindow() || GetDocument().IsContextSuspended() ||
      GetDocument().IsContextDestroyed())
    return;

  GetDocument().domWindow()->DispatchEvent(event);

  if (needs_checking_null_events_) {
    if (IsNullEvent(event))
      StopUpdating();
    else
      needs_checking_null_events_ = false;
  }
}

void DeviceSingleWindowEventController::DidAddEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName())
    return;

  if (GetPage() && GetPage()->IsPageVisible())
    StartUpdating();

  has_event_listener_ = true;
}

void DeviceSingleWindowEventController::DidRemoveEventListener(
    LocalDOMWindow* window,
    const AtomicString& event_type) {
  if (event_type != EventTypeName() ||
      window->HasEventListeners(EventTypeName()))
    return;

  StopUpdating();
  has_event_listener_ = false;
}

void DeviceSingleWindowEventController::DidRemoveAllEventListeners(
    LocalDOMWindow*) {
  StopUpdating();
  has_event_listener_ = false;
}

bool DeviceSingleWindowEventController::IsSameSecurityOriginAsMainFrame()
    const {
  if (!GetDocument().GetFrame() || !GetDocument().GetPage())
    return false;

  if (GetDocument().GetFrame()->IsMainFrame())
    return true;

  SecurityOrigin* main_security_origin = GetDocument()
                                             .GetPage()
                                             ->MainFrame()
                                             ->GetSecurityContext()
                                             ->GetSecurityOrigin();

  if (main_security_origin &&
      GetDocument().GetSecurityOrigin()->CanAccess(main_security_origin))
    return true;

  return false;
}

DEFINE_TRACE(DeviceSingleWindowEventController) {
  visitor->Trace(document_);
  PlatformEventController::Trace(visitor);
}

}  // namespace blink
