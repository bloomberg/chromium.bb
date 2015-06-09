// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector_chromeos.h"

#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "remoting/host/chromeos/point_transformer.h"
#include "remoting/host/clipboard.h"
#include "remoting/proto/internal.pb.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/system_input_injector.h"

namespace remoting {

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;
using protocol::TextEvent;
using protocol::TouchEvent;

namespace {

ui::EventFlags MouseButtonToUIFlags(MouseEvent::MouseButton button) {
  switch (button) {
    case MouseEvent::BUTTON_LEFT:
      return ui::EF_LEFT_MOUSE_BUTTON;
    case MouseEvent::BUTTON_RIGHT:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    case MouseEvent::BUTTON_MIDDLE:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    default:
      NOTREACHED();
      return ui::EF_NONE;
  }
}

}  // namespace

// This class is run exclusively on the UI thread of the browser process.
class InputInjectorChromeos::Core {
 public:
  Core();

  // Mirrors the public InputInjectorChromeos interface.
  void InjectClipboardEvent(const ClipboardEvent& event);
  void InjectKeyEvent(const KeyEvent& event);
  void InjectTextEvent(const TextEvent& event);
  void InjectMouseEvent(const MouseEvent& event);
  void Start(scoped_ptr<protocol::ClipboardStub> client_clipboard);

 private:
  scoped_ptr<ui::SystemInputInjector> delegate_;
  scoped_ptr<Clipboard> clipboard_;

  // Used to rotate the input coordinates appropriately based on the current
  // display rotation settings.
  scoped_ptr<PointTransformer> point_transformer_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

InputInjectorChromeos::Core::Core() {
}

void InputInjectorChromeos::Core::InjectClipboardEvent(
    const ClipboardEvent& event) {
  clipboard_->InjectClipboardEvent(event);
}

void InputInjectorChromeos::Core::InjectKeyEvent(const KeyEvent& event) {
  DCHECK(event.has_pressed());
  DCHECK(event.has_usb_keycode());

  ui::DomCode dom_code =
      ui::KeycodeConverter::UsbKeycodeToDomCode(event.usb_keycode());

  // Ignore events which can't be mapped.
  if (dom_code != ui::DomCode::NONE) {
    delegate_->InjectKeyEvent(dom_code, event.pressed(),
                              true /* suppress_auto_repeat */);
  }
}

void InputInjectorChromeos::Core::InjectTextEvent(const TextEvent& event) {
  // Chrome OS only supports It2Me, which is not supported on mobile clients, so
  // we don't need to implement text events.
  NOTIMPLEMENTED();
}

void InputInjectorChromeos::Core::InjectMouseEvent(const MouseEvent& event) {
  if (event.has_button() && event.has_button_down()) {
    delegate_->InjectMouseButton(MouseButtonToUIFlags(event.button()),
                                 event.button_down());
  } else if (event.has_wheel_delta_y() || event.has_wheel_delta_x()) {
    delegate_->InjectMouseWheel(event.wheel_delta_x(), event.wheel_delta_y());
  } else {
    DCHECK(event.has_x() && event.has_y());
    delegate_->MoveCursorTo(point_transformer_->ToScreenCoordinates(
        gfx::PointF(event.x(), event.y())));
  }
}

void InputInjectorChromeos::Core::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  ui::OzonePlatform* ozone_platform = ui::OzonePlatform::GetInstance();
  delegate_ = ozone_platform->CreateSystemInputInjector();
  DCHECK(delegate_);

  // Implemented by remoting::ClipboardAura.
  clipboard_ = Clipboard::Create();
  clipboard_->Start(client_clipboard.Pass());
  point_transformer_.reset(new PointTransformer());
}

InputInjectorChromeos::InputInjectorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : input_task_runner_(task_runner) {
  core_.reset(new Core());
}

InputInjectorChromeos::~InputInjectorChromeos() {
  input_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

void InputInjectorChromeos::InjectClipboardEvent(const ClipboardEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::InjectClipboardEvent,
                            base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectKeyEvent(const KeyEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::InjectKeyEvent, base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectTextEvent(const TextEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&Core::InjectTextEvent, base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectMouseEvent(const MouseEvent& event) {
  input_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::InjectMouseEvent,
                            base::Unretained(core_.get()), event));
}

void InputInjectorChromeos::InjectTouchEvent(const TouchEvent& event) {
  NOTIMPLEMENTED() << "Raw touch event injection not implemented for ChromeOS.";
}

void InputInjectorChromeos::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  input_task_runner_->PostTask(
      FROM_HERE, base::Bind(&Core::Start, base::Unretained(core_.get()),
                            base::Passed(&client_clipboard)));
}

// static
scoped_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  // The Ozone input injector must be called on the UI task runner of the
  // browser process.
  return make_scoped_ptr(new InputInjectorChromeos(ui_task_runner));
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
