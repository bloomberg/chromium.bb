// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_completion_callback.h"
#include "webkit/plugins/ppapi/audio_helper.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/resource_helper.h"

namespace webkit {
namespace ppapi {

// AudioHelper -----------------------------------------------------------------

AudioHelper::AudioHelper()
    : create_callback_pending_(false),
      shared_memory_size_for_create_callback_(0) {
  create_callback_ = PP_MakeCompletionCallback(NULL, NULL);
}

AudioHelper::~AudioHelper() {
  // If the completion callback hasn't fired yet, do so here
  // with an error condition.
  if (create_callback_pending_) {
    PP_RunCompletionCallback(&create_callback_, PP_ERROR_ABORTED);
    create_callback_pending_ = false;
  }
}

int32_t AudioHelper::GetSyncSocketImpl(int* sync_socket) {
  if (socket_for_create_callback_.get()) {
#if defined(OS_POSIX)
    *sync_socket = socket_for_create_callback_->handle();
#elif defined(OS_WIN)
    *sync_socket = reinterpret_cast<int>(socket_for_create_callback_->handle());
#else
    #error "Platform not supported."
#endif
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

int32_t AudioHelper::GetSharedMemoryImpl(int* shm_handle, uint32_t* shm_size) {
  if (shared_memory_for_create_callback_.get()) {
#if defined(OS_POSIX)
    *shm_handle = shared_memory_for_create_callback_->handle().fd;
#elif defined(OS_WIN)
    *shm_handle = reinterpret_cast<int>(
        shared_memory_for_create_callback_->handle());
#else
    #error "Platform not supported."
#endif
    *shm_size = shared_memory_size_for_create_callback_;
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

void AudioHelper::StreamCreated(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  if (create_callback_pending_) {
    // Trusted side of proxy can specify a callback to recieve handles. In
    // this case we don't need to map any data or start the thread since it
    // will be handled by the proxy.
    shared_memory_for_create_callback_.reset(
        new base::SharedMemory(shared_memory_handle, false));
    shared_memory_size_for_create_callback_ = shared_memory_size;
    socket_for_create_callback_.reset(new base::SyncSocket(socket_handle));

    PP_RunCompletionCallback(&create_callback_, 0);
    create_callback_pending_ = false;

    // It might be nice to close the handles here to free up some system
    // resources, but we can't since there's a race condition. The handles must
    // be valid until they're sent over IPC, which is done from the I/O thread
    // which will often get done after this code executes. We could do
    // something more elaborate like an ACK from the plugin or post a task to
    // the I/O thread and back, but this extra complexity doesn't seem worth it
    // just to clean up these handles faster.
  } else {
    OnSetStreamInfo(shared_memory_handle, shared_memory_size, socket_handle);
  }
}

void AudioHelper::SetCallbackInfo(bool create_callback_pending,
                                  PP_CompletionCallback create_callback) {
  create_callback_pending_ = create_callback_pending;
  create_callback_ = create_callback;
}

}  // namespace ppapi
}  // namespace webkit
