/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "modules/gamepad/NavigatorGamepad.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "modules/gamepad/GamepadDispatcher.h"
#include "modules/gamepad/GamepadEvent.h"
#include "modules/gamepad/GamepadList.h"

namespace blink {

template <typename T>
static void SampleGamepad(unsigned index,
                          T& gamepad,
                          const device::Gamepad& device_gamepad) {
  gamepad.SetId(device_gamepad.id);
  gamepad.SetIndex(index);
  gamepad.SetConnected(device_gamepad.connected);
  gamepad.SetTimestamp(device_gamepad.timestamp);
  gamepad.SetMapping(device_gamepad.mapping);
  gamepad.SetAxes(device_gamepad.axes_length, device_gamepad.axes);
  gamepad.SetButtons(device_gamepad.buttons_length, device_gamepad.buttons);
  gamepad.SetPose(device_gamepad.pose);
  gamepad.SetHand(device_gamepad.hand);
  gamepad.SetDisplayId(device_gamepad.display_id);
}

template <typename GamepadType, typename ListType>
static void SampleGamepads(ListType* into) {
  device::Gamepads gamepads;

  GamepadDispatcher::Instance().SampleGamepads(gamepads);

  for (unsigned i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    device::Gamepad& web_gamepad = gamepads.items[i];
    if (web_gamepad.connected) {
      GamepadType* gamepad = into->item(i);
      if (!gamepad)
        gamepad = GamepadType::Create();
      SampleGamepad(i, *gamepad, web_gamepad);
      into->Set(i, gamepad);
    } else {
      into->Set(i, 0);
    }
  }
}

NavigatorGamepad* NavigatorGamepad::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return 0;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorGamepad& NavigatorGamepad::From(Navigator& navigator) {
  NavigatorGamepad* supplement = static_cast<NavigatorGamepad*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorGamepad(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

GamepadList* NavigatorGamepad::getGamepads(Navigator& navigator) {
  return NavigatorGamepad::From(navigator).Gamepads();
}

GamepadList* NavigatorGamepad::Gamepads() {
  if (!gamepads_)
    gamepads_ = GamepadList::Create();
  if (StartUpdatingIfAttached())
    SampleGamepads<Gamepad>(gamepads_.Get());
  return gamepads_.Get();
}

DEFINE_TRACE(NavigatorGamepad) {
  visitor->Trace(gamepads_);
  visitor->Trace(pending_events_);
  visitor->Trace(dispatch_one_event_runner_);
  Supplement<Navigator>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  PlatformEventController::Trace(visitor);
}

bool NavigatorGamepad::StartUpdatingIfAttached() {
  Document* document = static_cast<Document*>(GetExecutionContext());
  // The frame must be attached to start updating.
  if (document && document->GetFrame()) {
    StartUpdating();
    return true;
  }
  return false;
}

void NavigatorGamepad::DidUpdateData() {
  // We should stop listening once we detached.
  Document* document = static_cast<Document*>(GetExecutionContext());
  DCHECK(document->GetFrame());
  DCHECK(document->GetFrame()->DomWindow());

  // We register to the dispatcher before sampling gamepads so we need to check
  // if we actually have an event listener.
  if (!has_event_listener_)
    return;

  if (document->IsContextDestroyed() || document->IsContextSuspended())
    return;

  const GamepadDispatcher::ConnectionChange& change =
      GamepadDispatcher::Instance().LatestConnectionChange();

  if (!gamepads_)
    gamepads_ = GamepadList::Create();

  Gamepad* gamepad = gamepads_->item(change.index);
  if (!gamepad)
    gamepad = Gamepad::Create();
  SampleGamepad(change.index, *gamepad, change.pad);
  gamepads_->Set(change.index, gamepad);

  pending_events_.push_back(gamepad);
  dispatch_one_event_runner_->RunAsync();
}

void NavigatorGamepad::DispatchOneEvent() {
  Document* document = static_cast<Document*>(GetExecutionContext());
  DCHECK(document->GetFrame());
  DCHECK(document->GetFrame()->DomWindow());
  DCHECK(!pending_events_.IsEmpty());

  Gamepad* gamepad = pending_events_.TakeFirst();
  const AtomicString& event_name = gamepad->connected()
                                       ? EventTypeNames::gamepadconnected
                                       : EventTypeNames::gamepaddisconnected;
  document->GetFrame()->DomWindow()->DispatchEvent(
      GamepadEvent::Create(event_name, false, true, gamepad));

  if (!pending_events_.IsEmpty())
    dispatch_one_event_runner_->RunAsync();
}

NavigatorGamepad::NavigatorGamepad(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      ContextLifecycleObserver(
          navigator.GetFrame() ? navigator.GetFrame()->GetDocument() : nullptr),
      PlatformEventController(navigator.GetFrame()),
      dispatch_one_event_runner_(AsyncMethodRunner<NavigatorGamepad>::Create(
          this,
          &NavigatorGamepad::DispatchOneEvent)) {
  if (navigator.GetFrame())
    navigator.GetFrame()->DomWindow()->RegisterEventListenerObserver(this);
}

NavigatorGamepad::~NavigatorGamepad() {}

const char* NavigatorGamepad::SupplementName() {
  return "NavigatorGamepad";
}

void NavigatorGamepad::ContextDestroyed(ExecutionContext*) {
  StopUpdating();
}

void NavigatorGamepad::RegisterWithDispatcher() {
  GamepadDispatcher::Instance().AddController(this);
  dispatch_one_event_runner_->Resume();
}

void NavigatorGamepad::UnregisterWithDispatcher() {
  dispatch_one_event_runner_->Suspend();
  GamepadDispatcher::Instance().RemoveController(this);
}

bool NavigatorGamepad::HasLastData() {
  // Gamepad data is polled instead of pushed.
  return false;
}

static bool IsGamepadEvent(const AtomicString& event_type) {
  return event_type == EventTypeNames::gamepadconnected ||
         event_type == EventTypeNames::gamepaddisconnected;
}

void NavigatorGamepad::DidAddEventListener(LocalDOMWindow*,
                                           const AtomicString& event_type) {
  if (IsGamepadEvent(event_type)) {
    if (GetPage() && GetPage()->IsPageVisible())
      StartUpdatingIfAttached();
    has_event_listener_ = true;
  }
}

void NavigatorGamepad::DidRemoveEventListener(LocalDOMWindow* window,
                                              const AtomicString& event_type) {
  if (IsGamepadEvent(event_type) &&
      !window->HasEventListeners(EventTypeNames::gamepadconnected) &&
      !window->HasEventListeners(EventTypeNames::gamepaddisconnected)) {
    DidRemoveGamepadEventListeners();
  }
}

void NavigatorGamepad::DidRemoveAllEventListeners(LocalDOMWindow*) {
  DidRemoveGamepadEventListeners();
}

void NavigatorGamepad::DidRemoveGamepadEventListeners() {
  has_event_listener_ = false;
  dispatch_one_event_runner_->Stop();
  pending_events_.clear();
}

void NavigatorGamepad::PageVisibilityChanged() {
  // Inform the embedder whether it needs to provide gamepad data for us.
  bool visible = GetPage()->IsPageVisible();
  if (visible && (has_event_listener_ || gamepads_))
    StartUpdatingIfAttached();
  else
    StopUpdating();

  if (!visible || !has_event_listener_)
    return;

  // Tell the page what has changed. m_gamepads contains the state before we
  // became hidden.  We create a new snapshot and compare them.
  GamepadList* old_gamepads = gamepads_.Release();
  Gamepads();
  GamepadList* new_gamepads = gamepads_.Get();
  DCHECK(new_gamepads);

  for (unsigned i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    Gamepad* old_gamepad = old_gamepads ? old_gamepads->item(i) : 0;
    Gamepad* new_gamepad = new_gamepads->item(i);
    bool old_was_connected = old_gamepad && old_gamepad->connected();
    bool new_is_connected = new_gamepad && new_gamepad->connected();
    bool connected_gamepad_changed = old_was_connected && new_is_connected &&
                                     old_gamepad->id() != new_gamepad->id();
    if (connected_gamepad_changed || (old_was_connected && !new_is_connected)) {
      old_gamepad->SetConnected(false);
      pending_events_.push_back(old_gamepad);
    }
    if (connected_gamepad_changed || (!old_was_connected && new_is_connected)) {
      pending_events_.push_back(new_gamepad);
    }
  }

  if (!pending_events_.IsEmpty())
    dispatch_one_event_runner_->RunAsync();
}

}  // namespace blink
