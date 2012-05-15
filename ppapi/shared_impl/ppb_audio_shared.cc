// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_audio_shared.h"

#include "base/logging.h"
#include "ppapi/shared_impl/ppapi_globals.h"

using base::subtle::Atomic32;

namespace ppapi {

// FIXME: The following two functions (TotalSharedMemorySizeInBytes,
// SetActualDataSizeInBytes) are copied from audio_util.cc.
// Remove these functions once a minimal media library is provided for them.
// code.google.com/p/chromium/issues/detail?id=123203

uint32 TotalSharedMemorySizeInBytes(uint32 packet_size) {
  // Need to reserve extra 4 bytes for size of data.
  return packet_size + sizeof(Atomic32);
}

void SetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                              uint32 shared_memory_size,
                              uint32 actual_data_size) {
  char* ptr = static_cast<char*>(shared_memory->memory()) + shared_memory_size;
  DCHECK_EQ(0u, reinterpret_cast<size_t>(ptr) & 3);

  // Set actual data size at the end of the buffer.
  base::subtle::Release_Store(reinterpret_cast<volatile Atomic32*>(ptr),
                              actual_data_size);
}

const int PPB_Audio_Shared::kPauseMark = -1;

PPB_Audio_Shared::PPB_Audio_Shared()
    : playing_(false),
      shared_memory_size_(0),
      callback_(NULL),
      user_data_(NULL) {
}

PPB_Audio_Shared::~PPB_Audio_Shared() {
  StopThread();
}

void PPB_Audio_Shared::SetCallback(PPB_Audio_Callback callback,
                                   void* user_data) {
  callback_ = callback;
  user_data_ = user_data;
}

void PPB_Audio_Shared::SetStartPlaybackState() {
  DCHECK(!playing_);
  DCHECK(!audio_thread_.get());

  // If the socket doesn't exist, that means that the plugin has started before
  // the browser has had a chance to create all the shared memory info and
  // notify us. This is a common case. In this case, we just set the playing_
  // flag and the playback will automatically start when that data is available
  // in SetStreamInfo.
  playing_ = true;
  StartThread();
}

void PPB_Audio_Shared::SetStopPlaybackState() {
  DCHECK(playing_);
  StopThread();
  playing_ = false;
}

void PPB_Audio_Shared::SetStreamInfo(
    PP_Instance instance,
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::CancelableSyncSocket(socket_handle));
  shared_memory_.reset(new base::SharedMemory(shared_memory_handle, false));
  shared_memory_size_ = shared_memory_size;

  if (!shared_memory_->Map(TotalSharedMemorySizeInBytes(shared_memory_size_))) {
    PpapiGlobals::Get()->LogWithSource(instance, PP_LOGLEVEL_WARNING, "",
      "Failed to map shared memory for PPB_Audio_Shared.");
  }

  StartThread();
}

void PPB_Audio_Shared::StartThread() {
  // Don't start the thread unless all our state is set up correctly.
  if (!playing_ || !callback_ || !socket_.get() || !shared_memory_->memory())
    return;
  DCHECK(!audio_thread_.get());
  audio_thread_.reset(new base::DelegateSimpleThread(
      this, "plugin_audio_thread"));
  audio_thread_->Start();
}

void PPB_Audio_Shared::StopThread() {
  // Shut down the socket to escape any hanging |Receive|s.
  if (socket_.get())
    socket_->Shutdown();
  if (audio_thread_.get()) {
    audio_thread_->Join();
    audio_thread_.reset();
  }
}

void PPB_Audio_Shared::Run() {
  int pending_data;
  void* buffer = shared_memory_->memory();

  while (sizeof(pending_data) ==
      socket_->Receive(&pending_data, sizeof(pending_data)) &&
      pending_data != kPauseMark) {
    callback_(buffer, shared_memory_size_, user_data_);

    // Let the host know we are done.
    SetActualDataSizeInBytes(shared_memory_.get(), shared_memory_size_,
        shared_memory_size_);
  }
}

}  // namespace ppapi
