// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "remoting/host/input_injector.h"

namespace remoting {

namespace {

class InputInjectorAndroid : public InputInjector {
 public:
  InputInjectorAndroid() {}

  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override {
    NOTIMPLEMENTED();
  }

  void InjectKeyEvent(const protocol::KeyEvent& event) override {
    NOTIMPLEMENTED();
  }

  void InjectTextEvent(const protocol::TextEvent& event) override {
    NOTIMPLEMENTED();
  }

  void InjectMouseEvent(const protocol::MouseEvent& event) override {
    NOTIMPLEMENTED();
  }

  void InjectTouchEvent(const protocol::TouchEvent& event) override {
    NOTIMPLEMENTED();
  }

  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InputInjectorAndroid);
};

}  // namespace

// static
std::unique_ptr<InputInjector> InputInjector::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return base::WrapUnique(new InputInjectorAndroid());
}

// static
bool InputInjector::SupportsTouchEvents() {
  return false;
}

}  // namespace remoting
