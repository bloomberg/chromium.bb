// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/manifest_service.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_message_filter.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "ppapi/nacl_irt/irt_manifest.h"
#include "ppapi/nacl_irt/plugin_startup.h"
#include "ppapi/proxy/ppapi_messages.h"

#if !defined(OS_NACL_SFI)
#include <pthread.h>
#include <map>
#include <string>
#endif

namespace ppapi {

// IPC channel is asynchronously set up. So, the NaCl process may try to
// send a OpenResource message to the host before the connection is
// established. In such a case, it is necessary to wait for the set up
// completion.
class ManifestMessageFilter : public IPC::SyncMessageFilter {
 public:
  ManifestMessageFilter(base::WaitableEvent* shutdown_event)
      : SyncMessageFilter(shutdown_event),
        connected_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  bool Send(IPC::Message* message) override {
    // Wait until set up is actually done.
    connected_event_.Wait();
    return SyncMessageFilter::Send(message);
  }

  // When set up is done, OnFilterAdded is called on IO thread. Unblocks the
  // Send().
  void OnFilterAdded(IPC::Channel* channel) override {
    SyncMessageFilter::OnFilterAdded(channel);
    connected_event_.Signal();
  }

  // If an error is found, unblocks the Send(), too, to return an error.
  void OnChannelError() override {
    SyncMessageFilter::OnChannelError();
    connected_event_.Signal();
  }

  // Similar to OnChannelError, unblocks the Send() on the channel closing.
  void OnChannelClosing() override {
    SyncMessageFilter::OnChannelClosing();
    connected_event_.Signal();
  }

 private:
  base::WaitableEvent connected_event_;

  DISALLOW_COPY_AND_ASSIGN(ManifestMessageFilter);
};

ManifestService::ManifestService(
    const IPC::ChannelHandle& handle,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    base::WaitableEvent* shutdown_event) {
  filter_ = new ManifestMessageFilter(shutdown_event);
  channel_ = IPC::ChannelProxy::Create(handle, IPC::Channel::MODE_SERVER,
                                       NULL,  // Listener
                                       io_task_runner.get(),
                                       base::ThreadTaskRunnerHandle::Get());
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
  uint64_t file_token_lo = 0;
  uint64_t file_token_hi = 0;
  if (!filter_->Send(new PpapiHostMsg_OpenResource(file, &file_token_lo,
                                                   &file_token_hi, &ipc_fd))) {
    LOG(ERROR) << "ManifestService::OpenResource failed:" << file;
    *fd = -1;
    return false;
  }

  // File tokens are used internally by NaClIPCAdapter and should have
  // been cleared from the message when it is received here.
  // These tokens should never be set for Non-SFI mode.
  CHECK(file_token_lo == 0);
  CHECK(file_token_hi == 0);

  // Copy the file if we received a valid file descriptor. Otherwise, if we got
  // a reply, the file doesn't exist, so provide an fd of -1.
  // See IrtOpenResource() for how this function's result is interpreted.
  if (ipc_fd.is_file())
    *fd = ipc_fd.descriptor().fd;
  else
    *fd = -1;
  return true;
}

#if !defined(OS_NACL_SFI)
namespace {

pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
std::map<std::string, int>* g_prefetched_fds;

}  // namespace

void RegisterPreopenedDescriptorsNonSfi(
    std::map<std::string, int>* key_fd_map) {
  pthread_mutex_lock(&g_mu);
  DCHECK(!g_prefetched_fds);
  g_prefetched_fds = key_fd_map;
  pthread_mutex_unlock(&g_mu);
}
#endif

int IrtOpenResource(const char* file, int* fd) {
  // Remove leading '/' character.
  if (file[0] == '/')
    ++file;

#if !defined(OS_NACL_SFI)
  // Fast path for prefetched FDs.
  pthread_mutex_lock(&g_mu);
  if (g_prefetched_fds) {
    std::map<std::string, int>::iterator it = g_prefetched_fds->find(file);
    if (it != g_prefetched_fds->end()) {
      *fd = it->second;
      g_prefetched_fds->erase(it);
      pthread_mutex_unlock(&g_mu);
      return 0;
    }
  }
  pthread_mutex_unlock(&g_mu);
#endif

  ManifestService* manifest_service = GetManifestService();
  if (manifest_service == NULL ||
      !manifest_service->OpenResource(file, fd)) {
    return NACL_ABI_EIO;
  }
  return (*fd == -1) ? NACL_ABI_ENOENT : 0;
}

}  // namespace ppapi
