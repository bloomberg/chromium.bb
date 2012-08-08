// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_audio_input_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::TrackedCallback;

namespace webkit {
namespace ppapi {

// PPB_AudioInput_Impl ---------------------------------------------------------

PPB_AudioInput_Impl::PPB_AudioInput_Impl(PP_Instance instance)
    : ::ppapi::PPB_AudioInput_Shared(instance),
      audio_input_(NULL) {
}

PPB_AudioInput_Impl::~PPB_AudioInput_Impl() {
  Close();
}

// static
PP_Resource PPB_AudioInput_Impl::Create0_1(
    PP_Instance instance,
    PP_Resource config,
    PPB_AudioInput_Callback audio_input_callback,
    void* user_data) {
  scoped_refptr<PPB_AudioInput_Impl>
      audio_input(new PPB_AudioInput_Impl(instance));
  int32_t result = audio_input->Open(
      "", config, audio_input_callback, user_data,
      MakeIgnoredCompletionCallback(audio_input.get()));
  if (result != PP_OK && result != PP_OK_COMPLETIONPENDING)
    return 0;
  return audio_input->GetReference();
}

int32_t PPB_AudioInput_Impl::OpenTrusted(
    const std::string& device_id,
    PP_Resource config,
    scoped_refptr<TrackedCallback> create_callback) {
  return CommonOpen(device_id, config, NULL, NULL, create_callback);
}

int32_t PPB_AudioInput_Impl::GetSyncSocket(int* sync_socket) {
  if (socket_.get()) {
#if defined(OS_POSIX)
    *sync_socket = socket_->handle();
#elif defined(OS_WIN)
    *sync_socket = reinterpret_cast<int>(socket_->handle());
#else
    #error "Platform not supported."
#endif
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

int32_t PPB_AudioInput_Impl::GetSharedMemory(int* shm_handle,
                                             uint32_t* shm_size) {
  if (shared_memory_.get()) {
#if defined(OS_POSIX)
    *shm_handle = shared_memory_->handle().fd;
#elif defined(OS_WIN)
    *shm_handle = reinterpret_cast<int>(shared_memory_->handle());
#else
    #error "Platform not supported."
#endif
    *shm_size = shared_memory_size_;
    return PP_OK;
  }
  return PP_ERROR_FAILED;
}

const PPB_AudioInput_Impl::DeviceRefDataVector&
    PPB_AudioInput_Impl::GetDeviceRefData() const {
  return devices_data_;
}

void PPB_AudioInput_Impl::StreamCreated(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket) {
  OnOpenComplete(PP_OK, shared_memory_handle, shared_memory_size, socket);
}

void PPB_AudioInput_Impl::StreamCreationFailed() {
  OnOpenComplete(PP_ERROR_FAILED, base::SharedMemory::NULLHandle(), 0,
                 base::SyncSocket::kInvalidHandle);
}

int32_t PPB_AudioInput_Impl::InternalEnumerateDevices(
    PP_Resource* devices,
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  devices_ = devices;
  enumerate_devices_callback_ = callback;
  plugin_delegate->EnumerateDevices(
      PP_DEVICETYPE_DEV_AUDIOCAPTURE,
      base::Bind(&PPB_AudioInput_Impl::EnumerateDevicesCallbackFunc,
                 AsWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_AudioInput_Impl::InternalOpen(
    const std::string& device_id,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count,
    scoped_refptr<TrackedCallback> callback) {
  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  // When the stream is created, we'll get called back on StreamCreated().
  DCHECK(!audio_input_);
  audio_input_ = plugin_delegate->CreateAudioInput(
      device_id, sample_rate, sample_frame_count, this);
  if (audio_input_) {
    open_callback_ = callback;
    return PP_OK_COMPLETIONPENDING;
  } else {
    return PP_ERROR_FAILED;
  }
}

PP_Bool PPB_AudioInput_Impl::InternalStartCapture() {
  if (!audio_input_)
    return PP_FALSE;
  SetStartCaptureState();
  audio_input_->StartCapture();
  return PP_TRUE;
}

PP_Bool PPB_AudioInput_Impl::InternalStopCapture() {
  if (!audio_input_)
    return PP_FALSE;
  audio_input_->StopCapture();
  SetStopCaptureState();
  return PP_TRUE;
}

void PPB_AudioInput_Impl::InternalClose() {
  // Calling ShutDown() makes sure StreamCreated() cannot be called anymore and
  // releases the audio data associated with the pointer. Note however, that
  // until ShutDown() returns, StreamCreated() may still be called. This will be
  // OK since OnOpenComplete() will clean up the handles and do nothing else in
  // that case.
  if (audio_input_) {
    audio_input_->ShutDown();
    audio_input_ = NULL;
  }
}

void PPB_AudioInput_Impl::EnumerateDevicesCallbackFunc(
    int request_id,
    bool succeeded,
    const DeviceRefDataVector& devices) {
  devices_data_.clear();
  if (succeeded)
    devices_data_ = devices;

  OnEnumerateDevicesComplete(succeeded ? PP_OK : PP_ERROR_FAILED, devices);
}

}  // namespace ppapi
}  // namespace webkit
