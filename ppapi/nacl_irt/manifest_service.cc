// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/manifest_service.h"

#include "base/message_loop/message_loop_proxy.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_message_filter.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "ppapi/nacl_irt/irt_manifest.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace ppapi {

const char kFilePrefix[] = "files/";

ManifestService::ManifestService(
    const IPC::ChannelHandle& handle,
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    base::WaitableEvent* shutdown_event) {
  filter_ = new IPC::SyncMessageFilter(shutdown_event);
  channel_.reset(new IPC::ChannelProxy(handle,
                                       IPC::Channel::MODE_SERVER,
                                       NULL,  // Listener
                                       io_message_loop));
  channel_->AddFilter(filter_.get());
}

ManifestService::~ManifestService() {
}

void ManifestService::StartupInitializationComplete() {
  filter_->Send(new PpapiHostMsg_StartupInitializationComplete);
}

bool ManifestService::OpenResource(const char* file, int* fd) {
  // OpenResource will return INVALID SerializedHandle, if it is not supported.
  // Specifically, PNaCl doesn't support open resource.
  ppapi::proxy::SerializedHandle ipc_fd;
  if (!filter_->Send(new PpapiHostMsg_OpenResource(
          std::string(kFilePrefix) + file, &ipc_fd)) ||
      !ipc_fd.is_file()) {
    LOG(ERROR) << "ManifestService::OpenResource failed:" << file;
    *fd = -1;
    return false;
  }

  *fd = ipc_fd.descriptor().fd;
  return true;
}

int IrtOpenResource(const char* file, int* fd) {
  // Remove leading '/' character.
  if (file[0] == '/')
    ++file;

  ManifestService* manifest_service = GetManifestService();
  if (manifest_service == NULL ||
      !manifest_service->OpenResource(file, fd)) {
    return NACL_ABI_EIO;
  }

  return (*fd == -1) ? NACL_ABI_ENOENT : 0;
}

}  // namespace ppapi
