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

// IPC channel is asynchronously set up. So, the NaCl process may try to
// send a OpenResource message to the host before the connection is
// established. In such a case, it is necessary to wait for the set up
// completion.
class ManifestMessageFilter : public IPC::SyncMessageFilter {
 public:
  ManifestMessageFilter(base::WaitableEvent* shutdown_event)
      : SyncMessageFilter(shutdown_event),
        connected_event_(
            true /* manual_reset */, false /* initially_signaled */) {
  }

  virtual bool Send(IPC::Message* message) OVERRIDE {
    // Wait until set up is actually done.
    connected_event_.Wait();
    return SyncMessageFilter::Send(message);
  }

  // When set up is done, OnFilterAdded is called on IO thread. Unblocks the
  // Send().
  virtual void OnFilterAdded(IPC::Sender* sender) OVERRIDE {
    SyncMessageFilter::OnFilterAdded(sender);
    connected_event_.Signal();
  }

  // If an error is found, unblocks the Send(), too, to return an error.
  virtual void OnChannelError() OVERRIDE {
    SyncMessageFilter::OnChannelError();
    connected_event_.Signal();
  }

  // Similar to OnChannelError, unblocks the Send() on the channel closing.
  virtual void OnChannelClosing() OVERRIDE {
    SyncMessageFilter::OnChannelClosing();
    connected_event_.Signal();
  }

 private:
  base::WaitableEvent connected_event_;

  DISALLOW_COPY_AND_ASSIGN(ManifestMessageFilter);
};

ManifestService::ManifestService(
    const IPC::ChannelHandle& handle,
    scoped_refptr<base::MessageLoopProxy> io_message_loop,
    base::WaitableEvent* shutdown_event) {
  filter_ = new ManifestMessageFilter(shutdown_event);
  channel_ = IPC::ChannelProxy::Create(handle,
                                       IPC::Channel::MODE_SERVER,
                                       NULL,  // Listener
                                       io_message_loop.get());
  channel_->AddFilter(filter_.get());
}

ManifestService::~ManifestService() {
}

void ManifestService::StartupInitializationComplete() {
  filter_->Send(new PpapiHostMsg_StartupInitializationComplete);
}

bool ManifestService::OpenResource(const char* file, int* fd) {
  // We currently restrict to only allow one concurrent open_resource() call
  // per plugin. This could be fixed by doing a token lookup with
  // NaClProcessMsg_ResolveFileTokenAsyncReply instead of using a
  // global inside components/nacl/loader/nacl_listener.cc
  base::AutoLock lock(open_resource_lock_);

  // OpenResource will return INVALID SerializedHandle, if it is not supported.
  // Specifically, PNaCl doesn't support open resource.
  ppapi::proxy::SerializedHandle ipc_fd;

  // File tokens are ignored here, but needed when the message is processed
  // inside NaClIPCAdapter.
  uint64_t file_token_lo;
  uint64_t file_token_hi;
  if (!filter_->Send(new PpapiHostMsg_OpenResource(
          std::string(kFilePrefix) + file,
          &ipc_fd,
          &file_token_lo,
          &file_token_hi))) {
    LOG(ERROR) << "ManifestService::OpenResource failed:" << file;
    *fd = -1;
    return false;
  }

#if defined(OS_NACL)
  // File tokens are used internally by NaClIPCAdapter and should have
  // been cleared from the message when it is received here.
  // Note that, on Non-SFI NaCl, the IPC channel is directly connected to the
  // renderer process, so NaClIPCAdapter does not work. It means,
  // file_token_{lo,hi} fields may be properly filled, although it is just
  // ignored here.
  CHECK(file_token_lo == 0);
  CHECK(file_token_hi == 0);
#endif

  // Copy the file if we received a valid file descriptor. Otherwise, if we got
  // a reply, the file doesn't exist, so provide an fd of -1.
  // See IrtOpenResource() for how this function's result is interpreted.
  if (ipc_fd.is_file())
    *fd = ipc_fd.descriptor().fd;
  else
    *fd = -1;
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
