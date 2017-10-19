/**
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "core/events/KeyboardEvent.h"

#include "build/build_config.h"
#include "core/editing/ime/InputMethodController.h"
#include "core/input/InputDeviceCapabilities.h"
#include "platform/WindowsKeyboardCodes.h"
#include "platform/bindings/DOMWrapperWorld.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInputEvent.h"

namespace blink {

namespace {

const AtomicString& EventTypeForKeyboardEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::kKeyUp:
      return EventTypeNames::keyup;
    case WebInputEvent::kRawKeyDown:
      return EventTypeNames::keydown;
    case WebInputEvent::kChar:
      return EventTypeNames::keypress;
    case WebInputEvent::kKeyDown:
      // The caller should disambiguate the combined event into RawKeyDown or
      // Char events.
      break;
    default:
      break;
  }
  NOTREACHED();
  return EventTypeNames::keydown;
}

KeyboardEvent::KeyLocationCode GetKeyLocationCode(const WebInputEvent& key) {
  if (key.GetModifiers() & WebInputEvent::kIsKeyPad)
    return KeyboardEvent::kDomKeyLocationNumpad;
  if (key.GetModifiers() & WebInputEvent::kIsLeft)
    return KeyboardEvent::kDomKeyLocationLeft;
  if (key.GetModifiers() & WebInputEvent::kIsRight)
    return KeyboardEvent::kDomKeyLocationRight;
  return KeyboardEvent::kDomKeyLocationStandard;
}

bool HasCurrentComposition(LocalDOMWindow* dom_window) {
  if (!dom_window)
    return false;
  LocalFrame* local_frame = dom_window->GetFrame();
  if (!local_frame)
    return false;
  return local_frame->GetInputMethodController().HasComposition();
}

}  // namespace

KeyboardEvent* KeyboardEvent::Create(ScriptState* script_state,
                                     const AtomicString& type,
                                     const KeyboardEventInit& initializer) {
  if (script_state->World().IsIsolatedWorld())
    UIEventWithKeyState::DidCreateEventInIsolatedWorld(
        initializer.ctrlKey(), initializer.altKey(), initializer.shiftKey(),
        initializer.metaKey());
  return new KeyboardEvent(type, initializer);
}

KeyboardEvent::KeyboardEvent() : location_(kDomKeyLocationStandard) {}

KeyboardEvent::KeyboardEvent(const WebKeyboardEvent& key,
                             LocalDOMWindow* dom_window)
    : UIEventWithKeyState(
          EventTypeForKeyboardEventType(key.GetType()),
          true,
          true,
          dom_window,
          0,
          static_cast<WebInputEvent::Modifiers>(key.GetModifiers()),
          TimeTicks::FromSeconds(key.TimeStampSeconds()),
          dom_window
              ? dom_window->GetInputDeviceCapabilities()->FiresTouchEvents(
                    false)
              : nullptr),
      key_event_(WTF::MakeUnique<WebKeyboardEvent>(key)),
      // TODO(crbug.com/482880): Fix this initialization to lazy initialization.
      code_(Platform::Current()->DomCodeStringFromEnum(key.dom_code)),
      key_(Platform::Current()->DomKeyStringFromEnum(key.dom_key)),
      location_(GetKeyLocationCode(key)),
      is_composing_(HasCurrentComposition(dom_window)) {
  InitLocationModifiers(location_);
}

KeyboardEvent::KeyboardEvent(const AtomicString& event_type,
                             const KeyboardEventInit& initializer)
    : UIEventWithKeyState(event_type, initializer),
      code_(initializer.code()),
      key_(initializer.key()),
      location_(initializer.location()),
      is_composing_(initializer.isComposing()) {
  if (initializer.repeat())
    modifiers_ |= WebInputEvent::kIsAutoRepeat;
  InitLocationModifiers(initializer.location());
}

KeyboardEvent::~KeyboardEvent() {}

void KeyboardEvent::initKeyboardEvent(ScriptState* script_state,
                                      const AtomicString& type,
                                      bool can_bubble,
                                      bool cancelable,
                                      AbstractView* view,
                                      const String& key_identifier,
                                      unsigned location,
                                      bool ctrl_key,
                                      bool alt_key,
                                      bool shift_key,
                                      bool meta_key) {
  if (IsBeingDispatched())
    return;

  if (script_state->World().IsIsolatedWorld())
    UIEventWithKeyState::DidCreateEventInIsolatedWorld(ctrl_key, alt_key,
                                                       shift_key, meta_key);

  initUIEvent(type, can_bubble, cancelable, view, 0);

  location_ = location;
  InitModifiers(ctrl_key, alt_key, shift_key, meta_key);
  InitLocationModifiers(location);
}

int KeyboardEvent::keyCode() const {
  // IE: virtual key code for keyup/keydown, character code for keypress
  // Firefox: virtual key code for keyup/keydown, zero for keypress
  // We match IE.
  if (!key_event_)
    return 0;

#if defined(OS_ANDROID)
  // FIXME: Check to see if this applies to other OS.
  // If the key event belongs to IME composition then propagate to JS.
  if (key_event_->native_key_code == 0xE5)  // VKEY_PROCESSKEY
    return key_event_->native_key_code;
#endif

  if (type() == EventTypeNames::keydown || type() == EventTypeNames::keyup)
    return key_event_->windows_key_code;

  return charCode();
}

int KeyboardEvent::charCode() const {
  // IE: not supported
  // Firefox: 0 for keydown/keyup events, character code for keypress
  // We match Firefox

  if (!key_event_ || (type() != EventTypeNames::keypress))
    return 0;
  return key_event_->text[0];
}

const AtomicString& KeyboardEvent::InterfaceName() const {
  return EventNames::KeyboardEvent;
}

bool KeyboardEvent::IsKeyboardEvent() const {
  return true;
}

unsigned KeyboardEvent::which() const {
  // Netscape's "which" returns a virtual key code for keydown and keyup, and a
  // character code for keypress.  That's exactly what IE's "keyCode" returns.
  // So they are the same for keyboard events.
  return (unsigned)keyCode();
}

void KeyboardEvent::InitLocationModifiers(unsigned location) {
  switch (location) {
    case KeyboardEvent::kDomKeyLocationNumpad:
      modifiers_ |= WebInputEvent::kIsKeyPad;
      break;
    case KeyboardEvent::kDomKeyLocationLeft:
      modifiers_ |= WebInputEvent::kIsLeft;
      break;
    case KeyboardEvent::kDomKeyLocationRight:
      modifiers_ |= WebInputEvent::kIsRight;
      break;
  }
}

void KeyboardEvent::Trace(blink::Visitor* visitor) {
  UIEventWithKeyState::Trace(visitor);
}

}  // namespace blink
