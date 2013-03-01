// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/rdp_desktop_session.h"

#include <winsock2.h>

#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/win/chromoting_module.h"

namespace remoting {

RdpDesktopSession::RdpDesktopSession() {
}

STDMETHODIMP RdpDesktopSession::Connect(
    long width,
    long height,
    IRdpDesktopSessionEventHandler* event_handler) {
  event_handler_ = event_handler;

  scoped_refptr<AutoThreadTaskRunner> task_runner =
      ChromotingModule::task_runner();
  DCHECK(task_runner->BelongsToCurrentThread());

  client_.reset(new RdpClient(task_runner, task_runner,
                              SkISize::Make(width, height), this));
  return S_OK;
}

STDMETHODIMP RdpDesktopSession::Disconnect() {
  client_.reset();
  event_handler_ = NULL;
  return S_OK;
}

STDMETHODIMP RdpDesktopSession::ChangeResolution(long width, long height) {
  return E_NOTIMPL;
}

void RdpDesktopSession::OnRdpConnected(const net::IPEndPoint& client_endpoint) {
  net::SockaddrStorage sockaddr;
  CHECK(client_endpoint.ToSockAddr(sockaddr.addr, &sockaddr.addr_len));

  event_handler_->OnRdpConnected(reinterpret_cast<byte*>(sockaddr.addr),
                                 sockaddr.addr_len);
}

void RdpDesktopSession::OnRdpClosed() {
  event_handler_->OnRdpClosed();
}

} // namespace remoting
