// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_audio_input_shared.h"

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"

namespace ppapi {

namespace {

void IgnoredCompletionCallback(void* user_data, int32_t result) {
  // Do nothing.
}

}  // namespace

PPB_AudioInput_Shared::PPB_AudioInput_Shared(const HostResource& audio_input)
    : Resource(OBJECT_IS_PROXY, audio_input),
      open_state_(BEFORE_OPEN),
      capturing_(false),
      shared_memory_size_(0),
      audio_input_callback_(NULL),
      user_data_(NULL),
      devices_(NULL),
      resource_object_type_(OBJECT_IS_PROXY) {
}

PPB_AudioInput_Shared::PPB_AudioInput_Shared(PP_Instance instance)
    : Resource(OBJECT_IS_IMPL, instance),
      open_state_(BEFORE_OPEN),
      capturing_(false),
      shared_memory_size_(0),
      audio_input_callback_(NULL),
      user_data_(NULL),
      devices_(NULL),
      resource_object_type_(OBJECT_IS_IMPL) {
}

PPB_AudioInput_Shared::~PPB_AudioInput_Shared() {
  DCHECK(open_state_ == CLOSED);
}

thunk::PPB_AudioInput_API* PPB_AudioInput_Shared::AsPPB_AudioInput_API() {
  return this;
}

int32_t PPB_AudioInput_Shared::EnumerateDevices(
    PP_Resource* devices,
    const PP_CompletionCallback& callback) {
  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;
  if (TrackedCallback::IsPending(enumerate_devices_callback_))
    return PP_ERROR_INPROGRESS;

  return InternalEnumerateDevices(devices, callback);
}

int32_t PPB_AudioInput_Shared::Open(
    const std::string& device_id,
    PP_Resource config,
    PPB_AudioInput_Callback audio_input_callback,
    void* user_data,
    const PP_CompletionCallback& callback) {
  if (!audio_input_callback)
    return PP_ERROR_BADARGUMENT;

  return CommonOpen(device_id, config, audio_input_callback, user_data,
                    callback);
}

PP_Resource PPB_AudioInput_Shared::GetCurrentConfig() {
  // AddRef for the caller.
  if (config_.get())
    PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool PPB_AudioInput_Shared::StartCapture() {
  if (open_state_ == CLOSED || (open_state_ == BEFORE_OPEN &&
                                !TrackedCallback::IsPending(open_callback_))) {
    return PP_FALSE;
  }
  if (capturing_)
    return PP_TRUE;

  // If the audio input device hasn't been opened, set |capturing_| to true and
  // return directly. Capturing will be started once the open operation is
  // completed.
  if (open_state_ == BEFORE_OPEN) {
    capturing_ = true;
    return PP_TRUE;
  }

  return InternalStartCapture();
}

PP_Bool PPB_AudioInput_Shared::StopCapture() {
  if (open_state_ == CLOSED)
    return PP_FALSE;
  if (!capturing_)
    return PP_TRUE;

  // If the audio input device hasn't been opened, set |capturing_| to false and
  // return directly.
  if (open_state_ == BEFORE_OPEN) {
    capturing_ = false;
    return PP_TRUE;
  }

  return InternalStopCapture();
}

void PPB_AudioInput_Shared::Close() {
  if (open_state_ == CLOSED)
    return;

  open_state_ = CLOSED;
  InternalClose();

  // Closing the socket causes the thread to exit - wait for it.
  if (socket_.get())
    socket_->Close();
  if (audio_input_thread_.get()) {
    audio_input_thread_->Join();
    audio_input_thread_.reset();
  }

  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->PostAbort();
}

void PPB_AudioInput_Shared::OnEnumerateDevicesComplete(
    int32_t result,
    const std::vector<DeviceRefData>& devices) {
  DCHECK(TrackedCallback::IsPending(enumerate_devices_callback_));

  if (result == PP_OK && devices_) {
    *devices_ = PPB_DeviceRef_Shared::CreateResourceArray(
        resource_object_type_, pp_instance(), devices);
  }
  devices_ = NULL;

  TrackedCallback::ClearAndRun(&enumerate_devices_callback_, result);
}

void PPB_AudioInput_Shared::OnOpenComplete(
    int32_t result,
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  if (open_state_ == BEFORE_OPEN && result == PP_OK) {
    open_state_ = OPENED;
    SetStreamInfo(shared_memory_handle, shared_memory_size, socket_handle);
  } else {
    // Clean up the handles.
    base::SyncSocket temp_socket(socket_handle);
    base::SharedMemory temp_mem(shared_memory_handle, false);
    capturing_ = false;
  }

  // The callback may have been aborted by Close().
  if (TrackedCallback::IsPending(open_callback_))
    TrackedCallback::ClearAndRun(&open_callback_, result);
}

// static
PP_CompletionCallback PPB_AudioInput_Shared::MakeIgnoredCompletionCallback() {
  return PP_MakeCompletionCallback(&IgnoredCompletionCallback, NULL);
}

void PPB_AudioInput_Shared::SetStartCaptureState() {
  DCHECK(!capturing_);
  DCHECK(!audio_input_thread_.get());

  if (audio_input_callback_ && socket_.get())
    StartThread();
  capturing_ = true;
}

void PPB_AudioInput_Shared::SetStopCaptureState() {
  DCHECK(capturing_);

  if (audio_input_thread_.get()) {
    audio_input_thread_->Join();
    audio_input_thread_.reset();
  }
  capturing_ = false;
}

void PPB_AudioInput_Shared::SetStreamInfo(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::SyncSocket(socket_handle));
  shared_memory_.reset(new base::SharedMemory(shared_memory_handle, false));
  shared_memory_size_ = shared_memory_size;

  if (audio_input_callback_)
    shared_memory_->Map(shared_memory_size_);

  // There is a pending capture request before SetStreamInfo().
  if (capturing_) {
    // Set |capturing_| to false so that the state looks consistent to
    // InternalStartCapture(). It will be reset to true by
    // SetStartCaptureState().
    capturing_ = false;
    InternalStartCapture();
  }
}

void PPB_AudioInput_Shared::StartThread() {
  DCHECK(audio_input_callback_);
  DCHECK(!audio_input_thread_.get());
  audio_input_thread_.reset(new base::DelegateSimpleThread(
      this, "plugin_audio_input_thread"));
  audio_input_thread_->Start();
}

void PPB_AudioInput_Shared::Run() {
  int pending_data;
  void* buffer = shared_memory_->memory();

  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
         pending_data >= 0) {
    audio_input_callback_(buffer, shared_memory_size_, user_data_);
  }
}

int32_t PPB_AudioInput_Shared::CommonOpen(
    const std::string& device_id,
    PP_Resource config,
    PPB_AudioInput_Callback audio_input_callback,
    void* user_data,
    PP_CompletionCallback callback) {
  if (open_state_ != BEFORE_OPEN)
    return PP_ERROR_FAILED;

  thunk::EnterResourceNoLock<thunk::PPB_AudioConfig_API> enter_config(config,
                                                                      true);
  if (enter_config.failed())
    return PP_ERROR_BADARGUMENT;

  if (!callback.func)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  if (TrackedCallback::IsPending(open_callback_))
    return PP_ERROR_INPROGRESS;

  config_ = config;
  audio_input_callback_ = audio_input_callback;
  user_data_ = user_data;

  return InternalOpen(device_id, enter_config.object()->GetSampleRate(),
                      enter_config.object()->GetSampleFrameCount(), callback);
}

}  // namespace ppapi
