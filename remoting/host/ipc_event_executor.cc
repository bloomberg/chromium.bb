// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ipc_event_executor.h"

#include "remoting/host/desktop_session_proxy.h"

namespace remoting {

IpcEventExecutor::IpcEventExecutor(
    scoped_refptr<DesktopSessionProxy> desktop_session_proxy)
    : desktop_session_proxy_(desktop_session_proxy) {
}

IpcEventExecutor::~IpcEventExecutor() {
}

void IpcEventExecutor::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  desktop_session_proxy_->InjectClipboardEvent(event);
}

void IpcEventExecutor::InjectKeyEvent(const protocol::KeyEvent& event) {
  desktop_session_proxy_->InjectKeyEvent(event);
}

void IpcEventExecutor::InjectMouseEvent(const protocol::MouseEvent& event) {
  desktop_session_proxy_->InjectMouseEvent(event);
}

void IpcEventExecutor::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  desktop_session_proxy_->StartEventExecutor(client_clipboard.Pass());
}

}  // namespace remoting
