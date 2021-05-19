// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/keyboard.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/exo/input_trace.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_modifiers.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/xkb_tracker.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace exo {
namespace {

// Delay until a key state change expected to be acknowledged is expired.
const int kExpirationDelayForPendingKeyAcksMs = 1000;

// The accelerator keys reserved to be processed by chrome.
const struct {
  ui::KeyboardCode keycode;
  int modifiers;
} kReservedAccelerators[] = {
    {ui::VKEY_F13, ui::EF_NONE},
    {ui::VKEY_I, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN},
    {ui::VKEY_Z, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN}};

bool ProcessAccelerator(Surface* surface, const ui::KeyEvent* event) {
  views::Widget* widget =
      views::Widget::GetTopLevelWidgetForNativeView(surface->window());
  if (widget) {
    views::FocusManager* focus_manager = widget->GetFocusManager();
    return focus_manager->ProcessAccelerator(ui::Accelerator(*event));
  }
  return false;
}

bool IsVirtualKeyboardEnabled() {
  return keyboard::GetAccessibilityKeyboardEnabled() ||
         keyboard::GetTouchKeyboardEnabled() ||
         (keyboard::KeyboardUIController::HasInstance() &&
          keyboard::KeyboardUIController::Get()->IsEnableFlagSet(
              keyboard::KeyboardEnableFlag::kCommandLineEnabled));
}

bool IsReservedAccelerator(const ui::KeyEvent* event) {
  for (const auto& accelerator : kReservedAccelerators) {
    if (event->flags() == accelerator.modifiers &&
        event->key_code() == accelerator.keycode) {
      return true;
    }
  }
  return false;
}

// Returns false if an accelerator is not reserved or it's not enabled.
bool ProcessAcceleratorIfReserved(Surface* surface, ui::KeyEvent* event) {
  return IsReservedAccelerator(event) && ProcessAccelerator(surface, event);
}

// Returns true if the surface needs to support IME.
// TODO(yhanada, https://crbug.com/847500): Remove this when we find a way
// to fix https://crbug.com/847500 without breaking ARC apps/Lacros browser.
bool IsImeSupportedSurface(Surface* surface) {
  aura::Window* window = surface->window();
  for (; window; window = window->parent()) {
    const auto app_type =
        static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
    switch (app_type) {
      case ash::AppType::ARC_APP:
      case ash::AppType::LACROS:
        return true;
      default:
        // Do nothing.
        break;
    }
  }
  return false;
}

// Returns true if the surface can consume ash accelerators.
bool CanConsumeAshAccelerators(Surface* surface) {
  aura::Window* window = surface->window();
  for (; window; window = window->parent()) {
    const auto app_type =
        static_cast<ash::AppType>(window->GetProperty(aura::client::kAppType));
    // TODO(fukino): Always returning false for Lacros window is a short-term
    // solution. In reality, Lacros can consume ash accelerator's key
    // combination when it is a deprecated ash accelerator or the window is
    // running PWA. We need to let the wayland client dynamically decrlare
    // whether it want to consume ash accelerators' key combinations.
    // crbug.com/1174025.
    if (app_type == ash::AppType::LACROS)
      return false;
  }
  return true;
}

// Returns true if an accelerator is an ash accelerator which can be handled
// before sending it to client and it is actually processed by ash-chrome.
bool ProcessAshAcceleratorIfPossible(Surface* surface, ui::KeyEvent* event) {
  // Process ash accelerators before sending it to client only when the client
  // should not consume ash accelerators. (e.g. Lacros-chrome)
  if (CanConsumeAshAccelerators(surface))
    return false;

  // Ctrl-N (new window), Shift-Ctrl-N (new incognite window), Ctrl-T (new tab),
  // and Shit-Ctrl-T (restore tab) need to be sent to the active client even
  // when the active window is lacros-chrome, since the ash-chrome does not
  // handle these new-window requests properly at this moment.
  // TODO(fukino): Remove this workaround once ash-chrome has an implementation
  // to handle new-window requests when lacros-chrome browser window is active.
  // crbug.com/1172189.
  const ui::Accelerator kAppHandlingAccelerators[] = {
      {ui::VKEY_N, ui::EF_CONTROL_DOWN},
      {ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
      {ui::VKEY_T, ui::EF_CONTROL_DOWN},
      {ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
  };
  ui::Accelerator accelerator(*event);
  if (base::Contains(kAppHandlingAccelerators, accelerator))
    return false;

  return ash::AcceleratorController::Get()->Process(accelerator);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Keyboard, public:

Keyboard::Keyboard(std::unique_ptr<KeyboardDelegate> delegate, Seat* seat)
    : delegate_(std::move(delegate)),
      seat_(seat),
      expiration_delay_for_pending_key_acks_(base::TimeDelta::FromMilliseconds(
          kExpirationDelayForPendingKeyAcksMs)) {
  AddEventHandler();
  seat_->AddObserver(this);
  ash::KeyboardController::Get()->AddObserver(this);
  ash::ImeControllerImpl* ime_controller = ash::Shell::Get()->ime_controller();
  ime_controller->AddObserver(this);

  delegate_->OnKeyboardLayoutUpdated(seat_->xkb_tracker()->GetKeymap().get());
  OnSurfaceFocused(seat_->GetFocusedSurface());
  OnKeyRepeatSettingsChanged(
      ash::KeyboardController::Get()->GetKeyRepeatSettings());
}

Keyboard::~Keyboard() {
  for (KeyboardObserver& observer : observer_list_)
    observer.OnKeyboardDestroying(this);
  if (focus_)
    focus_->RemoveSurfaceObserver(this);

  ash::Shell::Get()->ime_controller()->RemoveObserver(this);
  ash::KeyboardController::Get()->RemoveObserver(this);
  seat_->RemoveObserver(this);
  RemoveEventHandler();
}

bool Keyboard::HasDeviceConfigurationDelegate() const {
  return !!device_configuration_delegate_;
}

void Keyboard::SetDeviceConfigurationDelegate(
    KeyboardDeviceConfigurationDelegate* delegate) {
  device_configuration_delegate_ = delegate;
  OnKeyboardEnabledChanged(IsVirtualKeyboardEnabled());
}

void Keyboard::AddObserver(KeyboardObserver* observer) {
  observer_list_.AddObserver(observer);
}

bool Keyboard::HasObserver(KeyboardObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

void Keyboard::RemoveObserver(KeyboardObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void Keyboard::SetNeedKeyboardKeyAcks(bool need_acks) {
  RemoveEventHandler();
  are_keyboard_key_acks_needed_ = need_acks;
  AddEventHandler();
}

bool Keyboard::AreKeyboardKeyAcksNeeded() const {
  // Keyboard class doesn't need key acks while the spoken feedback is enabled.
  // While the spoken feedback is enabled, a key event is sent to both of a
  // wayland client and Chrome to give a chance to work to Chrome OS's
  // shortcuts.
  return are_keyboard_key_acks_needed_;
}

void Keyboard::AckKeyboardKey(uint32_t serial, bool handled) {
  auto it = pending_key_acks_.find(serial);
  if (it == pending_key_acks_.end())
    return;

  if (!handled && focus_)
    ProcessAccelerator(focus_, &it->second.first);
  pending_key_acks_.erase(serial);
}

////////////////////////////////////////////////////////////////////////////////
// ui::EventHandler overrides:

void Keyboard::OnKeyEvent(ui::KeyEvent* event) {
  if (!focus_)
    return;

  DCHECK(GetShellRootSurface(static_cast<aura::Window*>(event->target())) ||
         Surface::AsSurface(static_cast<aura::Window*>(event->target())));

  // Ignore synthetic key repeat events.
  if (event->is_repeat()) {
    // Clients should not see key repeat events and instead handle them on the
    // client side.
    // Mark the key repeat events as handled to avoid them from invoking
    // accelerators.
    event->SetHandled();
    return;
  }

  TRACE_EXO_INPUT_EVENT(event);

  // Process reserved accelerators or ash accelerators which need to be handled
  // before sending it to client.
  if (ProcessAcceleratorIfReserved(focus_, event) ||
      ProcessAshAcceleratorIfPossible(focus_, event)) {
    // Discard a key press event if the corresponding accelerator is handled.
    event->SetHandled();
    // The current focus might have been reset while processing accelerators.
    if (!focus_)
      return;
  }

  // When IME ate a key event, we use the event only for tracking key states and
  // ignore for further processing. Otherwise it is handled in two places (IME
  // and client) and causes undesired behavior.
  // If the window should receive a key event before IME, Exo should send any
  // key events to a client. The client will send back the events to IME if
  // needed.
  const bool consumed_by_ime =
      !focus_->window()->GetProperty(aura::client::kSkipImeProcessing) &&
      ConsumedByIme(focus_->window(), *event);

  // Always update modifiers.
  // XkbTracker must be updated in the Seat, before calling this method.
  // Ensured by the observer registration order.
  delegate_->OnKeyboardModifiers(seat_->xkb_tracker()->GetModifiers());

  // Currently, physical keycode is tracked in Seat, assuming that the
  // Keyboard::OnKeyEvent is called between Seat::WillProcessEvent and
  // Seat::DidProcessEvent. However, if IME is enabled, it is no longer true,
  // because IME work in async approach, and on its dispatching, call stack
  // is split so actually Keyboard::OnKeyEvent is called after
  // Seat::DidProcessEvent.
  // TODO(yhanada): This is a quick fix for https://crbug.com/859071. Remove
  // ARC-/Lacros-specific code path once we can find a way to manage
  // press/release events pair for synthetic events.
  ui::DomCode physical_code =
      seat_->physical_code_for_currently_processing_event();
  if (physical_code == ui::DomCode::NONE && focused_on_ime_supported_surface_) {
    // This key event is a synthetic event.
    // Consider DomCode field of the event as a physical code
    // for synthetic events when focus surface belongs to an ARC application.
    physical_code = event->code();
  }

  switch (event->type()) {
    case ui::ET_KEY_PRESSED: {
      auto it = pressed_keys_.find(physical_code);
      if (it == pressed_keys_.end() && !event->handled() &&
          physical_code != ui::DomCode::NONE) {
        for (auto& observer : observer_list_)
          observer.OnKeyboardKey(event->time_stamp(), event->code(), true);

        if (!consumed_by_ime) {
          // Process key press event if not already handled and not already
          // pressed.
          uint32_t serial = delegate_->OnKeyboardKey(event->time_stamp(),
                                                     event->code(), true);
          if (AreKeyboardKeyAcksNeeded()) {
            pending_key_acks_.insert(
                {serial,
                 {*event, base::TimeTicks::Now() +
                              expiration_delay_for_pending_key_acks_}});
            event->SetHandled();
          }
        }
        // Keep track of both the physical code and potentially re-written
        // code that this event generated.
        pressed_keys_.emplace(physical_code,
                              KeyState{event->code(), consumed_by_ime});
      } else if (it != pressed_keys_.end() && !event->handled()) {
        // Non-repeate key events for already pressed key can be sent in some
        // cases (e.g. Holding 'A' key then holding 'B' key then releasing 'A'
        // key sends a non-repeat 'B' key press event).
        // When it happens, we don't want to send the press event to a client
        // and also want to avoid it from invoking any accelerator.
        if (AreKeyboardKeyAcksNeeded())
          event->SetHandled();
      }
    } break;
    case ui::ET_KEY_RELEASED: {
      // Process key release event if currently pressed.
      auto it = pressed_keys_.find(physical_code);
      if (it != pressed_keys_.end()) {
        for (auto& observer : observer_list_)
          observer.OnKeyboardKey(event->time_stamp(), it->second.code, false);

        if (!it->second.consumed_by_ime) {
          // We use the code that was generated when the physical key was
          // pressed rather than the current event code. This allows events
          // to be re-written before dispatch, while still allowing the
          // client to track the state of the physical keyboard.
          uint32_t serial = delegate_->OnKeyboardKey(event->time_stamp(),
                                                     it->second.code, false);
          if (AreKeyboardKeyAcksNeeded()) {
            pending_key_acks_.insert(
                {serial,
                 {*event, base::TimeTicks::Now() +
                              expiration_delay_for_pending_key_acks_}});
            event->SetHandled();
          }
        }
        pressed_keys_.erase(it);
      }
    } break;
    default:
      NOTREACHED();
      break;
  }

  if (pending_key_acks_.empty())
    return;
  if (process_expired_pending_key_acks_pending_)
    return;

  ScheduleProcessExpiredPendingKeyAcks(expiration_delay_for_pending_key_acks_);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceObserver overrides:

void Keyboard::OnSurfaceDestroying(Surface* surface) {
  DCHECK(surface == focus_);
  SetFocus(nullptr);
}

////////////////////////////////////////////////////////////////////////////////
// SeatObserver overrides:

void Keyboard::OnSurfaceFocusing(Surface* gaining_focus) {}

void Keyboard::OnSurfaceFocused(Surface* gained_focus) {
  Surface* gained_focus_surface =
      gained_focus && delegate_->CanAcceptKeyboardEventsForSurface(gained_focus)
          ? gained_focus
          : nullptr;
  if (gained_focus_surface != focus_)
    SetFocus(gained_focus_surface);
}

////////////////////////////////////////////////////////////////////////////////
// ash::KeyboardControllerObserver overrides:

void Keyboard::OnKeyboardEnabledChanged(bool enabled) {
  if (device_configuration_delegate_) {
    // Ignore kAndroidDisabled which affects |enabled| and just test for a11y
    // and touch enabled keyboards. TODO(yhanada): Fix this using an Android
    // specific KeyboardUI implementation. https://crbug.com/897655.
    bool is_physical = !IsVirtualKeyboardEnabled();
    device_configuration_delegate_->OnKeyboardTypeChanged(is_physical);
  }
}

void Keyboard::OnKeyRepeatSettingsChanged(
    const ash::KeyRepeatSettings& settings) {
  delegate_->OnKeyRepeatSettingsChanged(settings.enabled, settings.delay,
                                        settings.interval);
}

////////////////////////////////////////////////////////////////////////////////
// ash::ImeControllerImpl::Observer overrides:

void Keyboard::OnCapsLockChanged(bool enabled) {}

void Keyboard::OnKeyboardLayoutNameChanged(const std::string& layout_name) {
  // XkbTracker must be updated in the Seat, before calling this method.
  // Ensured by the observer registration order.
  delegate_->OnKeyboardLayoutUpdated(seat_->xkb_tracker()->GetKeymap().get());
}

////////////////////////////////////////////////////////////////////////////////
// Keyboard, private:

void Keyboard::SetFocus(Surface* surface) {
  if (focus_) {
    delegate_->OnKeyboardLeave(focus_);
    focus_->RemoveSurfaceObserver(this);
    focus_ = nullptr;
    pending_key_acks_.clear();
  }
  if (surface) {
    pressed_keys_ = seat_->pressed_keys();
    delegate_->OnKeyboardModifiers(seat_->xkb_tracker()->GetModifiers());
    delegate_->OnKeyboardEnter(surface, pressed_keys_);
    focus_ = surface;
    focus_->AddSurfaceObserver(this);
    focused_on_ime_supported_surface_ = IsImeSupportedSurface(surface);
  }
}

void Keyboard::ProcessExpiredPendingKeyAcks() {
  DCHECK(process_expired_pending_key_acks_pending_);
  process_expired_pending_key_acks_pending_ = false;

  // Check pending acks and process them as if it is handled if
  // expiration time passed.
  base::TimeTicks current_time = base::TimeTicks::Now();

  while (!pending_key_acks_.empty()) {
    auto it = pending_key_acks_.begin();
    const ui::KeyEvent event = it->second.first;

    if (it->second.second > current_time)
      break;

    // Expiration time has passed, assume the event was handled.
    pending_key_acks_.erase(it);
  }

  if (pending_key_acks_.empty())
    return;

  base::TimeDelta delay_until_next_process_expired_pending_key_acks =
      pending_key_acks_.begin()->second.second - current_time;
  ScheduleProcessExpiredPendingKeyAcks(
      delay_until_next_process_expired_pending_key_acks);
}

void Keyboard::ScheduleProcessExpiredPendingKeyAcks(base::TimeDelta delay) {
  DCHECK(!process_expired_pending_key_acks_pending_);
  process_expired_pending_key_acks_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&Keyboard::ProcessExpiredPendingKeyAcks,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void Keyboard::AddEventHandler() {
  auto* helper = WMHelper::GetInstance();
  if (are_keyboard_key_acks_needed_)
    helper->AddPreTargetHandler(this);
  else
    helper->AddPostTargetHandler(this);
}

void Keyboard::RemoveEventHandler() {
  auto* helper = WMHelper::GetInstance();
  if (are_keyboard_key_acks_needed_)
    helper->RemovePreTargetHandler(this);
  else
    helper->RemovePostTargetHandler(this);
}

}  // namespace exo
