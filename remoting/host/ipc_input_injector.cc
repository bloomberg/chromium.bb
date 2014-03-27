// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_input_injector.h"

#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcInputInjector::IpcInputInjector(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {
}

IpcInputInjector::~IpcInputInjector() {
}

void IpcInputInjector::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  desktop_session_proxy_->InjectClipboardEvent(event);
}

void IpcInputInjector::InjectKeyEvent(const protocol::KeyEvent& event) {
  desktop_session_proxy_->InjectKeyEvent(event);
}

void IpcInputInjector::InjectTextEvent(const protocol::TextEvent& event) {
  desktop_session_proxy_->InjectTextEvent(event);
}

void IpcInputInjector::InjectMouseEvent(const protocol::MouseEvent& event) {
  desktop_session_proxy_->InjectMouseEvent(event);
}

void IpcInputInjector::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  desktop_session_proxy_->StartInputInjector(client_clipboard.Pass());
}

}  // namespace remoting
