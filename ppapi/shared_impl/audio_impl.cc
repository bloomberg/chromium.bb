// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_util.h"
#include "ppapi/shared_impl/audio_impl.h"

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

void AudioImpl::Run() {
  uint32 filled_size = media::GetMaxDataSizeInBytes(shared_memory_size_);
  AudioBuffersState buffer_state;

  while (buffer_state.Receive(socket_.get()) &&
         (buffer_state.total_bytes() >= 0)) {
    callback_(media::GetDataPointer(shared_memory_.get()),
              filled_size,
              user_data_);
    media::SetActualDataSizeInBytes(shared_memory_.get(), filled_size);
  }
}

}  // namespace ppapi
