// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "webkit/glue/plugins/plugin_web_event_converter_mac.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebInputEvent;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;

namespace {

// Returns true if the caps lock flag should be set for the given event.
// TODO: Ideally the event itself would know about the caps lock key; see
// <http://crbug.com/38226>. This function is only a temporary workaround that
// guesses based on live state.
bool CapsLockIsActive(const WebInputEvent& event) {
  NSUInteger current_flags = [[NSApp currentEvent] modifierFlags];
  bool caps_lock_on = (current_flags & NSAlphaShiftKeyMask) ? true : false;
  // If this a caps lock keypress, then the event stream state can be wrong.
  // Luckily, the weird event stream for caps lock makes it easy to tell whether
  // caps lock is being turned on or off.
  if (event.type == WebInputEvent::KeyDown ||
      event.type == WebInputEvent::KeyUp) {
    const WebKeyboardEvent* key_event =
        static_cast<const WebKeyboardEvent*>(&event);
    if (key_event->nativeKeyCode == 57)
      caps_lock_on = (event.type == WebInputEvent::KeyDown);
  }
  return caps_lock_on;
}

}  // namespace

#pragma mark -

#ifndef NP_NO_CARBON

// Converter implementation for the Carbon event model.
class CarbonPluginWebEventConverter : public PluginWebEventConverter {
 public:
  CarbonPluginWebEventConverter() {}
  virtual ~CarbonPluginWebEventConverter() {}

  virtual bool InitWithEvent(const WebInputEvent& web_event);

  virtual void* plugin_event() { return &carbon_event_; }

 protected:
  virtual bool ConvertKeyboardEvent(const WebKeyboardEvent& key_event);
  virtual bool ConvertMouseEvent(const WebMouseEvent& mouse_event);
  virtual bool ConvertMouseWheelEvent(const WebMouseWheelEvent& wheel_event);

 private:
  // Returns the Carbon translation of web_event's modifiers.
  static EventModifiers CarbonModifiers(const WebInputEvent& web_event);

  NPEvent carbon_event_;

  DISALLOW_COPY_AND_ASSIGN(CarbonPluginWebEventConverter);
};

bool CarbonPluginWebEventConverter::InitWithEvent(
    const WebInputEvent& web_event) {
  memset(&carbon_event_, 0, sizeof(carbon_event_));
  // Set the fields common to all event types.
  carbon_event_.when = TickCount();
  carbon_event_.modifiers |= CarbonModifiers(web_event);

  return PluginWebEventConverter::InitWithEvent(web_event);
}

bool CarbonPluginWebEventConverter::ConvertKeyboardEvent(
    const WebKeyboardEvent& key_event) {
  // TODO: Figure out how to handle Unicode input to plugins, if that's
  // even possible in the NPAPI Carbon event model.
  carbon_event_.message = (key_event.nativeKeyCode << 8) & keyCodeMask;
  carbon_event_.message |= key_event.text[0] & charCodeMask;
  carbon_event_.modifiers |= btnState;

  switch (key_event.type) {
    case WebInputEvent::KeyDown:
      if (key_event.modifiers & WebInputEvent::IsAutoRepeat)
        carbon_event_.what = autoKey;
      else
        carbon_event_.what = keyDown;
      return true;
    case WebInputEvent::KeyUp:
      carbon_event_.what = keyUp;
      return true;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::Char:
      // May be used eventually for IME, but currently not needed.
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool CarbonPluginWebEventConverter::ConvertMouseEvent(
    const WebMouseEvent& mouse_event) {
  carbon_event_.where.h = mouse_event.globalX;
  carbon_event_.where.v = mouse_event.globalY;

  // Default to "button up"; override this for mouse down events below.
  carbon_event_.modifiers |= btnState;

  switch (mouse_event.button) {
    case WebMouseEvent::ButtonLeft:
      break;
    case WebMouseEvent::ButtonMiddle:
      carbon_event_.modifiers |= cmdKey;
      break;
    case WebMouseEvent::ButtonRight:
      carbon_event_.modifiers |= controlKey;
      break;
    default:
      NOTIMPLEMENTED();
  }
  switch (mouse_event.type) {
    case WebInputEvent::MouseMove:
      carbon_event_.what = nullEvent;
      return true;
    case WebInputEvent::MouseLeave:
    case WebInputEvent::MouseEnter:
      carbon_event_.what = NPEventType_AdjustCursorEvent;
      return true;
    case WebInputEvent::MouseDown:
      carbon_event_.modifiers &= ~btnState;
      carbon_event_.what = mouseDown;
      return true;
    case WebInputEvent::MouseUp:
      carbon_event_.what = mouseUp;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool CarbonPluginWebEventConverter::ConvertMouseWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  return false;  // The Carbon NPAPI event model has no "mouse wheel" concept.
}

EventModifiers CarbonPluginWebEventConverter::CarbonModifiers(
    const WebInputEvent& web_event) {
  NSInteger modifiers = 0;
  if (web_event.modifiers & WebInputEvent::ControlKey)
    modifiers |= controlKey;
  if (web_event.modifiers & WebInputEvent::ShiftKey)
    modifiers |= shiftKey;
  if (web_event.modifiers & WebInputEvent::AltKey)
    modifiers |= optionKey;
  if (web_event.modifiers & WebInputEvent::MetaKey)
    modifiers |= cmdKey;
  if (CapsLockIsActive(web_event))
    modifiers |= alphaLock;
  return modifiers;
}

#endif  // !NP_NO_CARBON

#pragma mark -

// Converter implementation for the Cocoa event model.
class CocoaPluginWebEventConverter : public PluginWebEventConverter {
public:
  CocoaPluginWebEventConverter() {}
  virtual ~CocoaPluginWebEventConverter() {}

  virtual bool InitWithEvent(const WebInputEvent& web_event);

  virtual void* plugin_event() { return &cocoa_event_; }

protected:
  virtual bool ConvertKeyboardEvent(const WebKeyboardEvent& key_event);
  virtual bool ConvertMouseEvent(const WebMouseEvent& mouse_event);
  virtual bool ConvertMouseWheelEvent(const WebMouseWheelEvent& wheel_event);

private:
  // Returns the Cocoa translation of web_event's modifiers.
  static NSUInteger CocoaModifiers(const WebInputEvent& web_event);

  // Returns true if the given key is a modifier key.
  static bool KeyIsModifier(int native_key_code);

  NPCocoaEvent cocoa_event_;

  DISALLOW_COPY_AND_ASSIGN(CocoaPluginWebEventConverter);
};

bool CocoaPluginWebEventConverter::InitWithEvent(
    const WebInputEvent& web_event) {
  memset(&cocoa_event_, 0, sizeof(cocoa_event_));
  return PluginWebEventConverter::InitWithEvent(web_event);
}

bool CocoaPluginWebEventConverter::ConvertKeyboardEvent(
    const WebKeyboardEvent& key_event) {
  cocoa_event_.data.key.keyCode = key_event.nativeKeyCode;

  cocoa_event_.data.key.modifierFlags |= CocoaModifiers(key_event);

  // Modifier keys have their own event type, and don't get character or
  // repeat data.
  if (KeyIsModifier(key_event.nativeKeyCode)) {
    cocoa_event_.type = NPCocoaEventFlagsChanged;
    return true;
  }

  cocoa_event_.data.key.characters = reinterpret_cast<NPNSString*>(
      [NSString stringWithFormat:@"%S", key_event.text]);
  cocoa_event_.data.key.charactersIgnoringModifiers =
      reinterpret_cast<NPNSString*>(
          [NSString stringWithFormat:@"%S", key_event.unmodifiedText]);

  if (key_event.modifiers & WebInputEvent::IsAutoRepeat)
    cocoa_event_.data.key.isARepeat = true;

  switch (key_event.type) {
    case WebInputEvent::KeyDown:
      cocoa_event_.type = NPCocoaEventKeyDown;
      return true;
    case WebInputEvent::KeyUp:
      cocoa_event_.type = NPCocoaEventKeyUp;
      return true;
    case WebInputEvent::RawKeyDown:
    case WebInputEvent::Char:
      // May be used eventually for IME, but currently not needed.
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool CocoaPluginWebEventConverter::ConvertMouseEvent(
    const WebMouseEvent& mouse_event) {
  cocoa_event_.data.mouse.pluginX = mouse_event.x;
  cocoa_event_.data.mouse.pluginY = mouse_event.y;
  cocoa_event_.data.mouse.modifierFlags |= CocoaModifiers(mouse_event);
  cocoa_event_.data.mouse.clickCount = mouse_event.clickCount;
  switch (mouse_event.button) {
    case WebMouseEvent::ButtonLeft:
      cocoa_event_.data.mouse.buttonNumber = 0;
      break;
    case WebMouseEvent::ButtonMiddle:
      cocoa_event_.data.mouse.buttonNumber = 2;
      break;
    case WebMouseEvent::ButtonRight:
      cocoa_event_.data.mouse.buttonNumber = 1;
      break;
    default:
      cocoa_event_.data.mouse.buttonNumber = mouse_event.button;
      break;
  }
  switch (mouse_event.type) {
    case WebInputEvent::MouseDown:
      cocoa_event_.type = NPCocoaEventMouseDown;
      return true;
    case WebInputEvent::MouseUp:
      cocoa_event_.type = NPCocoaEventMouseUp;
      return true;
    case WebInputEvent::MouseMove: {
      bool mouse_is_down =
          (mouse_event.modifiers & WebInputEvent::LeftButtonDown) ||
          (mouse_event.modifiers & WebInputEvent::RightButtonDown) ||
          (mouse_event.modifiers & WebInputEvent::MiddleButtonDown);
      cocoa_event_.type = mouse_is_down ? NPCocoaEventMouseDragged
                                        : NPCocoaEventMouseMoved;
      return true;
    }
    case WebInputEvent::MouseEnter:
      cocoa_event_.type = NPCocoaEventMouseEntered;
      return true;
    case WebInputEvent::MouseLeave:
      cocoa_event_.type = NPCocoaEventMouseExited;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool CocoaPluginWebEventConverter::ConvertMouseWheelEvent(
    const WebMouseWheelEvent& wheel_event) {
  cocoa_event_.type = NPCocoaEventScrollWheel;
  cocoa_event_.data.mouse.pluginX = wheel_event.x;
  cocoa_event_.data.mouse.pluginY = wheel_event.y;
  cocoa_event_.data.mouse.modifierFlags |= CocoaModifiers(wheel_event);
  cocoa_event_.data.mouse.deltaX = wheel_event.deltaX;
  cocoa_event_.data.mouse.deltaY = wheel_event.deltaY;
  return true;
}

NSUInteger CocoaPluginWebEventConverter::CocoaModifiers(
    const WebInputEvent& web_event) {
  NSInteger modifiers = 0;
  if (web_event.modifiers & WebInputEvent::ControlKey)
    modifiers |= NSControlKeyMask;
  if (web_event.modifiers & WebInputEvent::ShiftKey)
    modifiers |= NSShiftKeyMask;
  if (web_event.modifiers & WebInputEvent::AltKey)
    modifiers |= NSAlternateKeyMask;
  if (web_event.modifiers & WebInputEvent::MetaKey)
    modifiers |= NSCommandKeyMask;
  if (CapsLockIsActive(web_event))
    modifiers |= NSAlphaShiftKeyMask;
  return modifiers;
}

bool CocoaPluginWebEventConverter::KeyIsModifier(int native_key_code) {
  switch (native_key_code) {
    case 55:  // Left command
    case 54:  // Right command
    case 58:  // Left option
    case 61:  // Right option
    case 59:  // Left control
    case 62:  // Right control
    case 56:  // Left shift
    case 60:  // Right shift
    case 57:  // Caps lock
      return true;
    default:
      return false;
  }
}

#pragma mark -

bool PluginWebEventConverter::InitWithEvent(const WebInputEvent& web_event) {
  if (web_event.type == WebInputEvent::MouseWheel) {
    return ConvertMouseWheelEvent(
        *static_cast<const WebMouseWheelEvent*>(&web_event));
  } else if (WebInputEvent::isMouseEventType(web_event.type)) {
    return ConvertMouseEvent(*static_cast<const WebMouseEvent*>(&web_event));
  } else if (WebInputEvent::isKeyboardEventType(web_event.type)) {
    return ConvertKeyboardEvent(
       *static_cast<const WebKeyboardEvent*>(&web_event));
  }
  DLOG(WARNING) << "Unknown event type " << web_event.type;
  return false;
}

#pragma mark -

PluginWebEventConverter*
    PluginWebEventConverterFactory::CreateConverterForModel(
        NPEventModel event_model) {
  switch (event_model) {
    case NPEventModelCocoa:
      return new CocoaPluginWebEventConverter();
#ifndef NP_NO_CARBON
    case NPEventModelCarbon:
      return new CarbonPluginWebEventConverter();
#endif
    default:
      NOTIMPLEMENTED();
      return NULL;
  }
}
