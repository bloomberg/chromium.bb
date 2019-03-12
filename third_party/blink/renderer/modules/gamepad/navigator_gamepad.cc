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

#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"

#include "base/auto_reset.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_comparisons.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_dispatcher.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_event.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad_list.h"
#include "third_party/blink/renderer/modules/vr/navigator_vr.h"

namespace blink {

namespace {

bool IsGamepadConnectionEvent(const AtomicString& event_type) {
  return event_type == event_type_names::kGamepadconnected ||
         event_type == event_type_names::kGamepaddisconnected;
}

bool HasConnectionEventListeners(LocalDOMWindow* window) {
  return window->HasEventListeners(event_type_names::kGamepadconnected) ||
         window->HasEventListeners(event_type_names::kGamepaddisconnected);
}

static void SampleGamepad(uint32_t index,
                          Gamepad& gamepad,
                          const device::Gamepad& device_gamepad,
                          const TimeTicks& navigation_start,
                          const TimeTicks& gamepads_start) {
  String old_id = gamepad.id();
  bool old_was_connected = gamepad.connected();

  TimeTicks last_updated =
      TimeTicks() + TimeDelta::FromMicroseconds(device_gamepad.timestamp);
  if (last_updated < gamepads_start)
    last_updated = gamepads_start;

  DOMHighResTimeStamp timestamp =
      Performance::MonotonicTimeToDOMHighResTimeStamp(navigation_start,
                                                      last_updated, false);
  gamepad.SetId(device_gamepad.id);
  gamepad.SetConnected(device_gamepad.connected);
  gamepad.SetTimestamp(timestamp);
  gamepad.SetAxes(device_gamepad.axes_length, device_gamepad.axes);
  gamepad.SetButtons(device_gamepad.buttons_length, device_gamepad.buttons);
  gamepad.SetPose(device_gamepad.pose);
  gamepad.SetHand(device_gamepad.hand);

  if (device_gamepad.is_xr) {
    TimeTicks now = TimeTicks::Now();
    TRACE_COUNTER1("input", "XR gamepad pose age (ms)",
                   (now - last_updated).InMilliseconds());
  }

  bool newly_connected;
  GamepadComparisons::HasGamepadConnectionChanged(
      old_was_connected, gamepad.connected(), old_id != gamepad.id(),
      &newly_connected, nullptr);

  // These fields are not expected to change and will only be written when the
  // gamepad is newly connected.
  if (newly_connected) {
    gamepad.SetIndex(index);
    gamepad.SetMapping(device_gamepad.mapping);
    gamepad.SetVibrationActuatorInfo(device_gamepad.vibration_actuator);
    // Re-map display ids, since we will hand out at most one VRDisplay.
    gamepad.SetDisplayId(device_gamepad.display_id ? 1 : 0);
  } else if (!gamepad.HasVibrationActuator() &&
             device_gamepad.vibration_actuator.not_null) {
    // Some gamepads require additional steps to determine haptics capability.
    // These gamepads may initially set |vibration_actuator| to null and then
    // update it some time later. Make sure such devices can correctly propagate
    // the changed capabilities.
    gamepad.SetVibrationActuatorInfo(device_gamepad.vibration_actuator);
  }
}

static void SampleGamepads(GamepadDispatcher* gamepad_dispatcher,
                           GamepadList* into,
                           ExecutionContext* context,
                           const TimeTicks& navigation_start,
                           const TimeTicks& gamepads_start) {
  device::Gamepads gamepads;

  gamepad_dispatcher->SampleGamepads(gamepads);

  for (uint32_t i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
    device::Gamepad& web_gamepad = gamepads.items[i];

    bool hide_xr_gamepad = false;
    if (web_gamepad.is_xr) {
      bool webxr_enabled =
          (context && origin_trials::WebXRGamepadSupportEnabled(context) &&
           origin_trials::WebXREnabled(context));
      bool webvr_enabled = (context && origin_trials::WebVREnabled(context));

      if (!webxr_enabled && !webvr_enabled) {
        // If neither WebXR nor WebVR are enabled, we should not expose XR-
        // backed gamepads.
        hide_xr_gamepad = true;
      }
    }

    if (hide_xr_gamepad) {
      into->Set(i, nullptr);
    } else if (web_gamepad.connected) {
      Gamepad* gamepad = into->item(i);
      if (!gamepad)
        gamepad = Gamepad::Create(context);
      SampleGamepad(i, *gamepad, web_gamepad, navigation_start, gamepads_start);
      into->Set(i, gamepad);
    } else {
      into->Set(i, nullptr);
    }
  }
}

}  // namespace

// static
const char NavigatorGamepad::kSupplementName[] = "NavigatorGamepad";

NavigatorGamepad* NavigatorGamepad::From(Document& document) {
  if (!document.GetFrame() || !document.GetFrame()->DomWindow())
    return nullptr;
  Navigator& navigator = *document.GetFrame()->DomWindow()->navigator();
  return &From(navigator);
}

NavigatorGamepad& NavigatorGamepad::From(Navigator& navigator) {
  NavigatorGamepad* supplement =
      Supplement<Navigator>::From<NavigatorGamepad>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorGamepad>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
GamepadList* NavigatorGamepad::getGamepads(Navigator& navigator) {
  return NavigatorGamepad::From(navigator).Gamepads();
}

GamepadList* NavigatorGamepad::Gamepads() {
  // Tell VR that gamepad is in use.
  Document* document = GetFrame() ? GetFrame()->GetDocument() : nullptr;
  if (document) {
    NavigatorVR* navigator_vr = NavigatorVR::From(*document);
    if (navigator_vr) {
      navigator_vr->SetDidUseGamepad();
    }
  }

  SampleAndCompareGamepadState();

  // Ensure |gamepads_| is not null.
  if (!gamepads_)
    gamepads_ = GamepadList::Create();

  // Allow gamepad button presses to qualify as user activations if the page is
  // visible.
  if (RuntimeEnabledFeatures::UserActivationV2Enabled() && GetFrame() &&
      GetPage() && GetPage()->IsPageVisible() &&
      GamepadComparisons::HasUserActivation(gamepads_)) {
    LocalFrame::NotifyUserActivation(GetFrame(), UserGestureToken::kNewGesture);
  }
  is_gamepads_exposed_ = true;

  return gamepads_.Get();
}

void NavigatorGamepad::Trace(blink::Visitor* visitor) {
  visitor->Trace(gamepads_);
  visitor->Trace(gamepads_back_);
  visitor->Trace(gamepad_dispatcher_);
  Supplement<Navigator>::Trace(visitor);
  DOMWindowClient::Trace(visitor);
  PlatformEventController::Trace(visitor);
}

bool NavigatorGamepad::StartUpdatingIfAttached() {
  // The frame must be attached to start updating.
  if (GetFrame()) {
    StartUpdating();
    return true;
  }
  return false;
}

void NavigatorGamepad::DidUpdateData() {
  // We should stop listening once we detached.
  DCHECK(GetFrame());
  DCHECK(DomWindow());

  // Record when gamepad data was first made available to the page.
  if (gamepads_start_.is_null())
    gamepads_start_ = base::TimeTicks::Now();

  // Fetch the new gamepad state and dispatch gamepad events.
  if (has_event_listener_)
    SampleAndCompareGamepadState();
}

NavigatorGamepad::NavigatorGamepad(Navigator& navigator)
    : Supplement<Navigator>(navigator),
      DOMWindowClient(navigator.DomWindow()),
      PlatformEventController(
          navigator.GetFrame() ? navigator.GetFrame()->GetDocument() : nullptr),
      // See https://bit.ly/2S0zRAS for task types
      gamepad_dispatcher_(MakeGarbageCollected<GamepadDispatcher>(
          navigator.GetFrame() ? navigator.GetFrame()->GetTaskRunner(
                                     blink::TaskType::kMiscPlatformAPI)
                               : nullptr)) {
  if (navigator.DomWindow())
    navigator.DomWindow()->RegisterEventListenerObserver(this);

  // Fetch |window.performance.timing.navigationStart|. Gamepad timestamps are
  // reported relative to this value.
  if (GetFrame()) {
    DocumentLoader* loader = GetFrame()->Loader().GetDocumentLoader();
    if (loader)
      navigation_start_ = loader->GetTiming().NavigationStart();
  }
}

NavigatorGamepad::~NavigatorGamepad() = default;

void NavigatorGamepad::RegisterWithDispatcher() {
  gamepad_dispatcher_->AddController(this);
}

void NavigatorGamepad::UnregisterWithDispatcher() {
  gamepad_dispatcher_->RemoveController(this);
}

bool NavigatorGamepad::HasLastData() {
  // Gamepad data is polled instead of pushed.
  return false;
}

void NavigatorGamepad::DidAddEventListener(LocalDOMWindow*,
                                           const AtomicString& event_type) {
  if (IsGamepadConnectionEvent(event_type)) {
    has_connection_event_listener_ = true;
    bool first_event_listener = !has_event_listener_;
    has_event_listener_ = true;

    if (GetPage() && GetPage()->IsPageVisible()) {
      StartUpdatingIfAttached();
      if (first_event_listener)
        SampleAndCompareGamepadState();
    }
  }
}

void NavigatorGamepad::DidRemoveEventListener(LocalDOMWindow* window,
                                              const AtomicString& event_type) {
  if (IsGamepadConnectionEvent(event_type)) {
    has_connection_event_listener_ = HasConnectionEventListeners(window);
    if (!has_connection_event_listener_)
      DidRemoveGamepadEventListeners();
  }
}

void NavigatorGamepad::DidRemoveAllEventListeners(LocalDOMWindow*) {
  DidRemoveGamepadEventListeners();
}

void NavigatorGamepad::DidRemoveGamepadEventListeners() {
  has_event_listener_ = false;
  StopUpdating();
}

void NavigatorGamepad::SwapGamepadBuffers() {
  // Swap the underlying buffers.
  gamepads_.Swap(gamepads_back_);

  // Preserve internal state when the buffers are swapped.
  for (unsigned i = 0; i < gamepads_->length(); ++i) {
    auto* gamepad_front = gamepads_->item(i);
    if (!gamepad_front)
      continue;
    const auto* gamepad_back =
        gamepads_back_ ? gamepads_back_->item(i) : nullptr;
    if (gamepad_back)
      gamepad_front->CopySharedStateFromBackBuffer(gamepad_back);
    else
      gamepad_front->InitializeSharedState();
  }
}

void NavigatorGamepad::SampleAndCompareGamepadState() {
  // Avoid re-entry. Do not fetch a new sample until we are finished dispatching
  // events from the previous sample.
  if (processing_events_)
    return;

  ExecutionContext* execution_context =
      DomWindow() ? DomWindow()->GetExecutionContext() : nullptr;

  base::AutoReset<bool>(&processing_events_, true);
  if (StartUpdatingIfAttached()) {
    if (GetPage()->IsPageVisible()) {
      // Allocate a buffer to hold the new gamepad state, if needed.
      if (!gamepads_back_)
        gamepads_back_ = GamepadList::Create();
      SampleGamepads(gamepad_dispatcher_, gamepads_back_.Get(),
                     execution_context, navigation_start_, gamepads_start_);

      // Compare the new sample with the previous sample and record which
      // gamepad events should be dispatched. Swap buffers if the gamepad
      // state changed. We must swap buffers before dispatching events to
      // ensure |gamepads_| holds the correct data when getGamepads is called
      // from inside a gamepad event listener.
      auto compare_result = GamepadComparisons::Compare(
          gamepads_.Get(), gamepads_back_.Get(), false, false);
      if (compare_result.IsDifferent()) {
        SwapGamepadBuffers();
        bool is_gamepads_back_exposed = is_gamepads_exposed_;
        is_gamepads_exposed_ = false;

        // Dispatch gamepad events. Dispatching an event calls the event
        // listeners synchronously.
        //
        // Note: In some instances the gamepad connection state may change while
        // inside an event listener. This is most common when using test APIs
        // that allow the gamepad state to be changed from javascript. The set
        // of event listeners may also change if listeners are added or removed
        // by another listener.
        for (uint32_t i = 0; i < device::Gamepads::kItemsLengthCap; ++i) {
          bool is_connected = compare_result.IsGamepadConnected(i);
          bool is_disconnected = compare_result.IsGamepadDisconnected(i);

          if (is_connected && is_disconnected) {
            // The newly-connected gamepad represents a different device than
            // the disconnected gamepad. Clear any shared state copied from the
            // back buffer.
            Gamepad* pad = gamepads_->item(i);
            DCHECK(pad);
            pad->InitializeSharedState();
          }

          // When a gamepad is disconnected and connected in the same update,
          // dispatch the gamepaddisconnected event first.
          if (has_connection_event_listener_ && is_disconnected) {
            Gamepad* pad = gamepads_back_->item(i);
            DCHECK(pad);
            pad->SetConnected(false);
            is_gamepads_back_exposed = true;
            DispatchGamepadEvent(event_type_names::kGamepaddisconnected, pad);
          }
          if (has_connection_event_listener_ && is_connected) {
            Gamepad* pad = gamepads_->item(i);
            DCHECK(pad);
            is_gamepads_exposed_ = true;
            DispatchGamepadEvent(event_type_names::kGamepadconnected, pad);
          }
        }

        // Clear |gamepads_back_| if it was ever exposed to the page so it can
        // be garbage collected when no active references remain. If it was
        // never exposed, retain the buffer so it can be reused.
        if (is_gamepads_back_exposed)
          gamepads_back_.Clear();
      }
    }
  }
}

void NavigatorGamepad::DispatchGamepadEvent(const AtomicString& event_name,
                                            Gamepad* gamepad) {
  DCHECK(has_connection_event_listener_);
  DCHECK(gamepad);
  DomWindow()->DispatchEvent(*GamepadEvent::Create(
      event_name, Event::Bubbles::kNo, Event::Cancelable::kYes, gamepad));
}

void NavigatorGamepad::PageVisibilityChanged() {
  // Inform the embedder whether it needs to provide gamepad data for us.
  bool visible = GetPage()->IsPageVisible();
  if (visible && (has_event_listener_ || gamepads_)) {
    StartUpdatingIfAttached();
  } else {
    StopUpdating();
  }

  if (visible && has_event_listener_)
    SampleAndCompareGamepadState();
}

}  // namespace blink
