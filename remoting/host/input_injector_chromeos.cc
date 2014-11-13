// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_injector_chromeos.h"

#include "base/logging.h"
#include "remoting/proto/internal.pb.h"

namespace remoting {

using protocol::ClipboardEvent;
using protocol::KeyEvent;
using protocol::MouseEvent;
using protocol::TextEvent;

// TODO(kelvinp): Implement this class (See crbug.com/426716).
InputInjectorChromeos::InputInjectorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : input_task_runner_(task_runner) {
  NOTIMPLEMENTED();
}

InputInjectorChromeos::~InputInjectorChromeos() {
  NOTIMPLEMENTED();
}

void InputInjectorChromeos::InjectClipboardEvent(const ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

void InputInjectorChromeos::InjectKeyEvent(const KeyEvent& event) {
  NOTIMPLEMENTED();
}

void InputInjectorChromeos::InjectTextEvent(const TextEvent& event) {
  NOTIMPLEMENTED();
}

void InputInjectorChromeos::InjectMouseEvent(const MouseEvent& event) {
  NOTIMPLEMENTED();
}

void InputInjectorChromeos::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  NOTIMPLEMENTED();
}

// static
scoped_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  scoped_ptr<InputInjectorChromeos> injector(new InputInjectorChromeos(
      ui_task_runner));
  return injector.Pass();
}

}  // namespace remoting
