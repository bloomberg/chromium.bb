// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/manifest_service.h"

#include "base/message_loop/message_loop_proxy.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_message_filter.h"

namespace ppapi {

ManifestService::ManifestService(
    const IPC::ChannelHandle& handle,
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    base::WaitableEvent* shutdown_event) {
  filter_ = new IPC::SyncMessageFilter(shutdown_event);
  channel_.reset(new IPC::ChannelProxy(handle,
                                       IPC::Channel::MODE_SERVER,
                                       NULL, // Listener
                                       io_message_loop));
  channel_->AddFilter(filter_.get());
}

ManifestService::~ManifestService() {
}

}  // namespace ppapi
