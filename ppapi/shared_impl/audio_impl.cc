// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/audio_impl.h"

#include "base/atomicops.h"
#include "base/logging.h"

using base::subtle::Atomic32;

namespace ppapi {

AudioImpl::AudioImpl()
    : playing_(false),
      shared_memory_size_(0),
      callback_(NULL),
      user_data_(NULL) {
}

AudioImpl::~AudioImpl() {
  // Closing the socket causes the thread to exit - wait for it.
  if (socket_.get())
    socket_->Close();
  if (audio_thread_.get()) {
    audio_thread_->Join();
    audio_thread_.reset();
  }
}

::ppapi::thunk::PPB_Audio_API* AudioImpl::AsPPB_Audio_API() {
  return this;
}

void AudioImpl::SetCallback(PPB_Audio_Callback callback, void* user_data) {
  callback_ = callback;
  user_data_ = user_data;
}

void AudioImpl::SetStartPlaybackState() {
  DCHECK(!playing_);
  DCHECK(!audio_thread_.get());

  // If the socket doesn't exist, that means that the plugin has started before
  // the browser has had a chance to create all the shared memory info and
  // notify us. This is a common case. In this case, we just set the playing_
  // flag and the playback will automatically start when that data is available
  // in SetStreamInfo.
  if (callback_ && socket_.get())
    StartThread();
  playing_ = true;
}

void AudioImpl::SetStopPlaybackState() {
  DCHECK(playing_);

  if (audio_thread_.get()) {
    audio_thread_->Join();
    audio_thread_.reset();
  }
  playing_ = false;
}

void AudioImpl::SetStreamInfo(base::SharedMemoryHandle shared_memory_handle,
                              size_t shared_memory_size,
                              base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::SyncSocket(socket_handle));
  shared_memory_.reset(new base::SharedMemory(shared_memory_handle, false));
  shared_memory_size_ = shared_memory_size;

  if (callback_) {
    shared_memory_->Map(shared_memory_size_);

    // In common case StartPlayback() was called before StreamCreated().
    if (playing_)
      StartThread();
  }
}

void AudioImpl::StartThread() {
  DCHECK(callback_);
  DCHECK(!audio_thread_.get());
  audio_thread_.reset(new base::DelegateSimpleThread(
      this, "plugin_audio_thread"));
  audio_thread_->Start();
}

// PPB_AudioBuffersState defines the type of an audio buffer state structure
// sent when requesting to fill the audio buffer with data.
typedef struct {
  int pending_bytes;
  int hardware_delay_bytes;
  uint64_t timestamp;
} PPB_AudioBuffersState;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PPB_AudioBuffersState, 16);

void AudioImpl::Run() {
  PPB_AudioBuffersState buffer_state;

  for (;;) {
    int bytes_received = socket_->Receive(&buffer_state, sizeof(buffer_state));
    if (bytes_received == sizeof(int)) {
      // Accoding to C++ Standard, address of structure == address of its first
      // member, so it is safe to use buffer_state.bytes_received when we
      // receive just one int.
      COMPILE_ASSERT(
          offsetof(PPB_AudioBuffersState, pending_bytes) == 0,
          pending_bytes_should_be_first_member_of_PPB_AudioBuffersState);
      if (buffer_state.pending_bytes < 0)
        break;
      callback_(shared_memory_->memory(), shared_memory_size_, user_data_);
    } else if (bytes_received == sizeof(buffer_state)) {
      if (buffer_state.pending_bytes + buffer_state.hardware_delay_bytes < 0)
        break;

      // Assert that shared memory is properly aligned, so we can cast pointer
      // to the pointer to Atomic32.
      DCHECK_EQ(0u, reinterpret_cast<size_t>(shared_memory_->memory()) & 3);

      // TODO(enal): Instead of address arithmetics use functions that
      // encapsulate internal buffer structure. They should be moved somewhere
      // into ppapi, right now they are defined in media/audio/audio_util.h.
      uint32 data_size = shared_memory_size_ - sizeof(Atomic32);
      callback_(static_cast<char*>(shared_memory_->memory()) + sizeof(Atomic32),
                data_size,
                user_data_);
      base::subtle::Release_Store(
          static_cast<volatile Atomic32*>(shared_memory_->memory()),
          data_size);
    } else {
      break;
    }
  }
}

}  // namespace ppapi
