// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/audio_input_impl.h"

#include "base/logging.h"

namespace ppapi {

AudioInputImpl::AudioInputImpl()
    : capturing_(false),
      shared_memory_size_(0),
      callback_(NULL),
      user_data_(NULL) {
}

AudioInputImpl::~AudioInputImpl() {
  // Closing the socket causes the thread to exit - wait for it.
  if (socket_.get())
    socket_->Close();
  if (audio_input_thread_.get()) {
    audio_input_thread_->Join();
    audio_input_thread_.reset();
  }
}

void AudioInputImpl::SetCallback(PPB_AudioInput_Callback callback,
                                 void* user_data) {
  callback_ = callback;
  user_data_ = user_data;
}

void AudioInputImpl::SetStartCaptureState() {
  DCHECK(!capturing_);
  DCHECK(!audio_input_thread_.get());

  // If the socket doesn't exist, that means that the plugin has started before
  // the browser has had a chance to create all the shared memory info and
  // notify us. This is a common case. In this case, we just set the playing_
  // flag and the capture will automatically start when that data is available
  // in SetStreamInfo.
  if (socket_.get())
    StartThread();
  capturing_ = true;
}

void AudioInputImpl::SetStopCaptureState() {
  DCHECK(capturing_);

  if (audio_input_thread_.get()) {
    audio_input_thread_->Join();
    audio_input_thread_.reset();
  }
  capturing_ = false;
}

void AudioInputImpl::SetStreamInfo(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::SyncSocket(socket_handle));
  shared_memory_.reset(new base::SharedMemory(shared_memory_handle, false));
  shared_memory_size_ = shared_memory_size;

  if (callback_) {
    shared_memory_->Map(shared_memory_size_);

    // In common case StartCapture() was called before StreamCreated().
    if (capturing_)
      StartThread();
  }
}

void AudioInputImpl::StartThread() {
  DCHECK(callback_);
  DCHECK(!audio_input_thread_.get());
  audio_input_thread_.reset(new base::DelegateSimpleThread(
      this, "plugin_audio_input_thread"));
  audio_input_thread_->Start();
}

void AudioInputImpl::Run() {
  int pending_data;
  void* buffer = shared_memory_->memory();

  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
         pending_data >= 0) {
    callback_(buffer, shared_memory_size_, user_data_);
  }
}

}  // namespace ppapi
