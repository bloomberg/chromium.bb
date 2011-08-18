// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/proxy_channel.h"

#include "ipc/ipc_platform_file.h"
#include "ipc/ipc_test_sink.h"

namespace ppapi {
namespace proxy {

ProxyChannel::ProxyChannel(base::ProcessHandle remote_process_handle)
    : delegate_(NULL),
      remote_process_handle_(remote_process_handle),
      test_sink_(NULL) {
}

ProxyChannel::~ProxyChannel() {
}

bool ProxyChannel::InitWithChannel(Delegate* delegate,
                                   const IPC::ChannelHandle& channel_handle,
                                   bool is_client) {
  delegate_ = delegate;
  IPC::Channel::Mode mode = is_client ? IPC::Channel::MODE_CLIENT
                                      : IPC::Channel::MODE_SERVER;
  channel_.reset(new IPC::SyncChannel(channel_handle, mode, this,
                                      delegate->GetIPCMessageLoop(), true,
                                      delegate->GetShutdownEvent()));
  return true;
}

void ProxyChannel::InitWithTestSink(IPC::TestSink* test_sink) {
  DCHECK(!test_sink_);
  test_sink_ = test_sink;
}

void ProxyChannel::OnChannelError() {
  channel_.reset();
}

#if defined(OS_POSIX)
int ProxyChannel::GetRendererFD() {
  DCHECK(channel());
  return channel()->GetClientFileDescriptor();
}
#endif

IPC::PlatformFileForTransit ProxyChannel::ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source) {
  return IPC::GetFileHandleForProcess(handle, remote_process_handle_,
                                      should_close_source);
}

bool ProxyChannel::Send(IPC::Message* msg) {
  if (test_sink_)
    return test_sink_->Send(msg);
  if (channel_.get())
    return channel_->Send(msg);

  // Remote side crashed, drop this message.
  delete msg;
  return false;
}

}  // namespace proxy
}  // namespace ppapi
