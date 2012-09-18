// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWER_VIEWER_IPC_SERVER_H_
#define UI_VIEWER_VIEWER_IPC_SERVER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ipc/ipc_sender.h"

// This class handles IPC commands for the Viewer process.
class ViewerIPCServer : public IPC::Listener, public IPC::Sender {
 public:
  explicit ViewerIPCServer(const IPC::ChannelHandle& handle);
  virtual ~ViewerIPCServer();

  bool Init();

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  IPC::SyncChannel* channel() { return channel_.get(); }

  // Safe to call on any thread, as long as it's guaranteed that the thread's
  // lifetime is less than the main thread.
  IPC::SyncMessageFilter* sync_message_filter() { return sync_message_filter_; }

 private:
  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // IPC message handlers.
  void OnPbufferHandle(const std::string& todo);

  // Helper method to create the sync channel.
  void CreateChannel();

  bool client_connected_;

  IPC::ChannelHandle channel_handle_;
  scoped_ptr<IPC::SyncChannel> channel_;

  // Allows threads other than the main thread to send sync messages.
  scoped_refptr<IPC::SyncMessageFilter> sync_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(ViewerIPCServer);
};

#endif  // UI_VIEWER_VIEWER_IPC_SERVER_H_
