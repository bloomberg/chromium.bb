// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/viewer/viewer_ipc_server.h"

#include "ui/viewer/viewer_messages.h"
#include "ui/viewer/viewer_process.h"
#include "ipc/ipc_logging.h"

ViewerIPCServer::ViewerIPCServer(const IPC::ChannelHandle& channel_handle)
    : client_connected_(false), channel_handle_(channel_handle) {
}

ViewerIPCServer::~ViewerIPCServer() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(NULL);
#endif

  channel_->RemoveFilter(sync_message_filter_.get());
}

bool ViewerIPCServer::Init() {
#ifdef IPC_MESSAGE_LOG_ENABLED
  IPC::Logging::GetInstance()->SetIPCSender(this);
#endif
  sync_message_filter_ =
      new IPC::SyncMessageFilter(g_viewer_process->shutdown_event());
  CreateChannel();
  return true;
}

bool ViewerIPCServer::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

bool ViewerIPCServer::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  // When we get a message, always mark the client as connected. The
  // ChannelProxy::Context is only letting OnChannelConnected get called once,
  // so on the Mac and Linux, we never would set client_connected_ to true
  // again on subsequent connections.
  client_connected_ = true;
  IPC_BEGIN_MESSAGE_MAP(ViewerIPCServer, msg)
    IPC_MESSAGE_HANDLER(ViewerMsg_PbufferHandle,
                        OnPbufferHandle)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ViewerIPCServer::OnChannelConnected(int32 peer_pid) {
  DCHECK(!client_connected_);
  client_connected_ = true;
}

void ViewerIPCServer::OnChannelError() {
  // When a client (typically a browser process) disconnects, the pipe is
  // closed and we get an OnChannelError. Since we want to keep servicing
  // client requests, we will recreate the channel.
  bool client_was_connected = client_connected_;
  client_connected_ = false;
  if (client_was_connected) {
    if (g_viewer_process->HandleClientDisconnect()) {
#if defined(OS_WIN)
      // On Windows, once an error on a named pipe occurs, the named pipe is no
      // longer valid and must be re-created. This is not the case on Mac or
      // Linux.
      CreateChannel();
#endif
    }
  } else {
    // If the client was never even connected we had an error connecting.
    if (!client_connected_) {
      LOG(ERROR) << "Unable to open viewer ipc channel "
                 << "named: " << channel_handle_.name;
    }
  }
}

void ViewerIPCServer::OnPbufferHandle(const std::string& todo) {
  NOTREACHED();
}

void ViewerIPCServer::CreateChannel() {
  channel_.reset(NULL); // Tear down the existing channel, if any.
  channel_.reset(new IPC::SyncChannel(channel_handle_,
      IPC::Channel::MODE_NAMED_SERVER, this,
      g_viewer_process->io_message_loop_proxy(), true,
      g_viewer_process->shutdown_event()));
  DCHECK(sync_message_filter_.get());
  channel_->AddFilter(sync_message_filter_.get());
}
