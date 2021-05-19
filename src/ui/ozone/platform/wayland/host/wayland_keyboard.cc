// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_keyboard.h"

#include <utility>

#include "base/files/scoped_file.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/unguessable_token.h"
#include "ui/base/buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

#if BUILDFLAG(USE_XKBCOMMON)
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#endif

namespace ui {

class WaylandKeyboard::ZCRExtendedKeyboard {
 public:
  // Takes the ownership of |extended_keyboard|.
  ZCRExtendedKeyboard(WaylandKeyboard* keyboard,
                      zcr_extended_keyboard_v1* extended_keyboard)
      : keyboard_(keyboard), obj_(extended_keyboard) {
    static constexpr zcr_extended_keyboard_v1_listener kListener = {
        &ZCRExtendedKeyboard::PeekKey,
    };
    zcr_extended_keyboard_v1_add_listener(obj_.get(), &kListener, this);
  }
  ZCRExtendedKeyboard(const ZCRExtendedKeyboard&) = delete;
  ZCRExtendedKeyboard& operator=(const ZCRExtendedKeyboard&) = delete;
  ~ZCRExtendedKeyboard() = default;

  void AckKey(uint32_t serial, bool handled) {
    zcr_extended_keyboard_v1_ack_key(obj_.get(), serial, handled);
  }

  // Returns true if connected object will send zcr_extended_keyboard::peek_key.
  bool IsPeekKeySupported() {
    return wl::get_version_of_object(obj_.get()) >=
           ZCR_EXTENDED_KEYBOARD_V1_PEEK_KEY_SINCE_VERSION;
  }

 private:
  static void PeekKey(void* data,
                      zcr_extended_keyboard_v1* obj,
                      uint32_t serial,
                      uint32_t time,
                      uint32_t key,
                      uint32_t state) {
    auto* extended_keyboard = static_cast<ZCRExtendedKeyboard*>(data);
    DCHECK(data);
    extended_keyboard->keyboard_->OnKey(
        serial, time, key, state, WaylandKeyboard::KeyEventKind::kPeekKey);
  }

  WaylandKeyboard* const keyboard_;
  wl::Object<zcr_extended_keyboard_v1> obj_;
};

// static
const wl_callback_listener WaylandKeyboard::callback_listener_ = {
    WaylandKeyboard::SyncCallback,
};

WaylandKeyboard::WaylandKeyboard(
    wl_keyboard* keyboard,
    zcr_keyboard_extension_v1* keyboard_extension_v1,
    WaylandConnection* connection,
    KeyboardLayoutEngine* layout_engine,
    Delegate* delegate)
    : obj_(keyboard),
      connection_(connection),
      delegate_(delegate),
      auto_repeat_handler_(this),
      layout_engine_(static_cast<LayoutEngine*>(layout_engine)) {
  static const wl_keyboard_listener listener = {
      &WaylandKeyboard::Keymap,    &WaylandKeyboard::Enter,
      &WaylandKeyboard::Leave,     &WaylandKeyboard::Key,
      &WaylandKeyboard::Modifiers, &WaylandKeyboard::RepeatInfo,
  };

  wl_keyboard_add_listener(obj_.get(), &listener, this);
  // TODO(tonikitoo): Default auto-repeat to ON here?

  if (keyboard_extension_v1) {
    extended_keyboard_ = std::make_unique<ZCRExtendedKeyboard>(
        this, zcr_keyboard_extension_v1_get_extended_keyboard(
                  keyboard_extension_v1, obj_.get()));
  }
}

WaylandKeyboard::~WaylandKeyboard() {
  // Reset keyboard modifiers on destruction.
  delegate_->OnKeyboardModifiersChanged(0);
}

void WaylandKeyboard::Keymap(void* data,
                             wl_keyboard* obj,
                             uint32_t format,
                             int32_t keymap_fd,
                             uint32_t size) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  base::ScopedFD fd(keymap_fd);
  auto length = size - 1;
  auto shmen = base::subtle::PlatformSharedMemoryRegion::Take(
      std::move(fd), base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
      length, base::UnguessableToken::Create());
  auto mapped_memory =
      base::UnsafeSharedMemoryRegion::Deserialize(std::move(shmen)).Map();
  const char* keymap = mapped_memory.GetMemoryAs<char>();

  if (!keymap || format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    return;

  bool success = keyboard->layout_engine_->SetCurrentLayoutFromBuffer(
      keymap, mapped_memory.size());
  DCHECK(success) << "Failed to set the XKB keyboard mapping.";
}

void WaylandKeyboard::Enter(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface,
                            wl_array* keys) {
  // wl_surface might have been destroyed by this time.
  if (auto* window = wl::RootWindowFromWlSurface(surface)) {
    auto* self = static_cast<WaylandKeyboard*>(data);
    self->delegate_->OnKeyboardFocusChanged(window, /*focused=*/true);
  }
}

void WaylandKeyboard::Leave(void* data,
                            wl_keyboard* obj,
                            uint32_t serial,
                            wl_surface* surface) {
  // wl_surface might have been destroyed by this time.
  auto* self = static_cast<WaylandKeyboard*>(data);
  if (auto* window = wl::RootWindowFromWlSurface(surface))
    self->delegate_->OnKeyboardFocusChanged(window, /*focused=*/false);

  // Upon window focus lose, reset the key repeat timers.
  self->auto_repeat_handler_.StopKeyRepeat();
}

void WaylandKeyboard::Key(void* data,
                          wl_keyboard* obj,
                          uint32_t serial,
                          uint32_t time,
                          uint32_t key,
                          uint32_t state) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);
  keyboard->OnKey(serial, time, key, state, KeyEventKind::kKey);
}

void WaylandKeyboard::Modifiers(void* data,
                                wl_keyboard* obj,
                                uint32_t serial,
                                uint32_t depressed,
                                uint32_t latched,
                                uint32_t locked,
                                uint32_t group) {
#if BUILDFLAG(USE_XKBCOMMON)
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  int modifiers = keyboard->layout_engine_->UpdateModifiers(depressed, latched,
                                                            locked, group);
  keyboard->delegate_->OnKeyboardModifiersChanged(modifiers);
#endif
}

void WaylandKeyboard::RepeatInfo(void* data,
                                 wl_keyboard* obj,
                                 int32_t rate,
                                 int32_t delay) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);

  keyboard->auto_repeat_handler_.SetAutoRepeatRate(
      base::TimeDelta::FromMilliseconds(delay),
      base::TimeDelta::FromMilliseconds(rate));
}

void WaylandKeyboard::FlushInput(base::OnceClosure closure) {
  if (sync_callback_)
    return;

  auto_repeat_closure_ = std::move(closure);

  // wl_display_sync gives a chance for any key "up" events to arrive.
  // With a well behaved wayland compositor this should ensure we never
  // get spurious repeats.
  sync_callback_.reset(wl_display_sync(connection_->display()));
  wl_callback_add_listener(sync_callback_.get(), &callback_listener_, this);
  connection_->ScheduleFlush();
}

void WaylandKeyboard::DispatchKey(unsigned int key,
                                  unsigned int scan_code,
                                  bool down,
                                  bool repeat,
                                  base::TimeTicks timestamp,
                                  int device_id,
                                  int flags) {
  // Key repeat is only triggered by wl_keyboard::key event,
  // but not by extended_keyboard::peek_key.
  DispatchKey(key, scan_code, down, repeat, timestamp, device_id, flags,
              KeyEventKind::kKey);
}

void WaylandKeyboard::OnKey(uint32_t serial,
                            uint32_t time,
                            uint32_t key,
                            uint32_t state,
                            KeyEventKind kind) {
  bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  if (down)
    connection_->set_serial(serial, ET_KEY_PRESSED);

  if (kind == KeyEventKind::kKey) {
    auto_repeat_handler_.UpdateKeyRepeat(key, 0 /*scan_code*/, down,
                                         false /*suppress_auto_repeat*/,
                                         device_id());
  }

  // Block to dispatch RELEASE wl_keyboard::key event, if
  // zcr_extended_keyboard::peek_key is supported, since the event is
  // already dispatched.
  // If not supported, dispatch it for compatibility.
  if (kind == KeyEventKind::kKey && !down && extended_keyboard_ &&
      extended_keyboard_->IsPeekKeySupported()) {
    return;
  }

  // TODO(tonikitoo,msisov): Handler 'repeat' parameter below.
  DispatchKey(key, 0 /*scan_code*/, down, false /*repeat*/, EventTimeForNow(),
              device_id(), EF_NONE, kind);
}

void WaylandKeyboard::DispatchKey(unsigned int key,
                                  unsigned int scan_code,
                                  bool down,
                                  bool repeat,
                                  base::TimeTicks timestamp,
                                  int device_id,
                                  int flags,
                                  KeyEventKind kind) {
  DomCode dom_code = KeycodeConverter::EvdevCodeToDomCode(key);
  if (dom_code == ui::DomCode::NONE)
    return;

  // Pass empty DomKey and KeyboardCode here so the delegate can pre-process
  // and decode it when needed.
  uint32_t result = delegate_->OnKeyboardKeyEvent(
      down ? ET_KEY_PRESSED : ET_KEY_RELEASED, dom_code, repeat, timestamp,
      device_id, kind);

  if (extended_keyboard_) {
    bool handled = result & POST_DISPATCH_STOP_PROPAGATION;
    extended_keyboard_->AckKey(connection_->serial(), handled);
  }
}

void WaylandKeyboard::SyncCallback(void* data,
                                   struct wl_callback* cb,
                                   uint32_t time) {
  WaylandKeyboard* keyboard = static_cast<WaylandKeyboard*>(data);
  DCHECK(keyboard);
  DCHECK(keyboard->auto_repeat_closure_);
  std::move(keyboard->auto_repeat_closure_).Run();
  keyboard->sync_callback_.reset();
}

}  // namespace ui
