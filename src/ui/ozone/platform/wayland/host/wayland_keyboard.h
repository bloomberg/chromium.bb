// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_KEYBOARD_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_KEYBOARD_H_

#include <cstdint>

#include "base/time/time.h"
#include "ui/base/buildflags.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/ozone/keyboard/event_auto_repeat_handler.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class KeyboardLayoutEngine;
class WaylandConnection;
class WaylandWindow;
#if BUILDFLAG(USE_XKBCOMMON)
class XkbKeyboardLayoutEngine;
#endif

class WaylandKeyboard : public EventAutoRepeatHandler::Delegate {
 public:
  class Delegate;
  class ZCRExtendedKeyboard;

  enum class KeyEventKind {
    kPeekKey,  // Originated by extended_keyboard::peek_key.
    kKey,      // Originated by wl_keyboard::key.
  };

  WaylandKeyboard(wl_keyboard* keyboard,
                  zcr_keyboard_extension_v1* keyboard_extension_v1,
                  WaylandConnection* connection,
                  KeyboardLayoutEngine* keyboard_layout_engine,
                  Delegate* delegate);
  virtual ~WaylandKeyboard();

  uint32_t id() const { return obj_.id(); }
  int device_id() const { return obj_.id(); }

 private:
  using LayoutEngine =
#if BUILDFLAG(USE_XKBCOMMON)
      XkbKeyboardLayoutEngine
#else
      KeyboardLayoutEngine
#endif
      ;

  // wl_keyboard_listener
  static void Keymap(void* data,
                     wl_keyboard* obj,
                     uint32_t format,
                     int32_t fd,
                     uint32_t size);
  static void Enter(void* data,
                    wl_keyboard* obj,
                    uint32_t serial,
                    wl_surface* surface,
                    wl_array* keys);
  static void Leave(void* data,
                    wl_keyboard* obj,
                    uint32_t serial,
                    wl_surface* surface);
  static void Key(void* data,
                  wl_keyboard* obj,
                  uint32_t serial,
                  uint32_t time,
                  uint32_t key,
                  uint32_t state);
  static void Modifiers(void* data,
                        wl_keyboard* obj,
                        uint32_t serial,
                        uint32_t mods_depressed,
                        uint32_t mods_latched,
                        uint32_t mods_locked,
                        uint32_t group);
  static void RepeatInfo(void* data,
                         wl_keyboard* obj,
                         int32_t rate,
                         int32_t delay);

  static void SyncCallback(void* data, struct wl_callback* cb, uint32_t time);

  // Callback for wl_keyboard::key and extended_keyboard::peek_key.
  void OnKey(uint32_t serial,
             uint32_t time,
             uint32_t key,
             uint32_t state,
             KeyEventKind kind);

  // Dispatches the key event.
  void DispatchKey(unsigned int key,
                   unsigned int scan_code,
                   bool down,
                   bool repeat,
                   base::TimeTicks timestamp,
                   int device_id,
                   int flags,
                   KeyEventKind kind);

  // EventAutoRepeatHandler::Delegate
  void FlushInput(base::OnceClosure closure) override;
  void DispatchKey(unsigned int key,
                   unsigned int scan_code,
                   bool down,
                   bool repeat,
                   base::TimeTicks timestamp,
                   int device_id,
                   int flags) override;

  wl::Object<wl_keyboard> obj_;
  std::unique_ptr<ZCRExtendedKeyboard> extended_keyboard_;
  WaylandConnection* const connection_;
  Delegate* const delegate_;

  // Key repeat handler.
  static const wl_callback_listener callback_listener_;
  EventAutoRepeatHandler auto_repeat_handler_;
  base::OnceClosure auto_repeat_closure_;
  wl::Object<wl_callback> sync_callback_;

  LayoutEngine* layout_engine_;
};

class WaylandKeyboard::Delegate {
 public:
  virtual void OnKeyboardFocusChanged(WaylandWindow* window, bool focused) = 0;
  virtual void OnKeyboardModifiersChanged(int modifiers) = 0;
  // Returns a mask of ui::PostDispatchAction indicating how the event was
  // dispatched.
  virtual uint32_t OnKeyboardKeyEvent(EventType type,
                                      DomCode dom_code,
                                      bool repeat,
                                      base::TimeTicks timestamp,
                                      int device_id,
                                      WaylandKeyboard::KeyEventKind kind) = 0;

 protected:
  // Prevent deletion through a WaylandKeyboard::Delegate pointer.
  virtual ~Delegate() = default;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_KEYBOARD_H_
