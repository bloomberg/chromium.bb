// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/audio_input_resource.h"

#include "base/bind.h"
#include "base/logging.h"
#include "ipc/ipc_platform_file.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/shared_memory_util.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"

namespace ppapi {
namespace proxy {

AudioInputResource::AudioInputResource(
    Connection connection,
    PP_Instance instance)
    : PluginResource(connection, instance),
      open_state_(BEFORE_OPEN),
      capturing_(false),
      shared_memory_size_(0),
      audio_input_callback_(NULL),
      user_data_(NULL),
      pending_enumerate_devices_(false) {
  SendCreate(RENDERER, PpapiHostMsg_AudioInput_Create());
}

AudioInputResource::~AudioInputResource() {
  Close();
}

thunk::PPB_AudioInput_API* AudioInputResource::AsPPB_AudioInput_API() {
  return this;
}

int32_t AudioInputResource::EnumerateDevices(
    PP_Resource* devices,
    scoped_refptr<TrackedCallback> callback) {
  if (pending_enumerate_devices_)
    return PP_ERROR_INPROGRESS;
  if (!devices)
    return PP_ERROR_BADARGUMENT;

  pending_enumerate_devices_ = true;
  PpapiHostMsg_AudioInput_EnumerateDevices msg;
  Call<PpapiPluginMsg_AudioInput_EnumerateDevicesReply>(
      RENDERER, msg,
      base::Bind(&AudioInputResource::OnPluginMsgEnumerateDevicesReply,
                 base::Unretained(this), devices, callback));
  return PP_OK_COMPLETIONPENDING;
}

int32_t AudioInputResource::Open(const std::string& device_id,
                                 PP_Resource config,
                                 PPB_AudioInput_Callback audio_input_callback,
                                 void* user_data,
                                 scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(open_callback_))
    return PP_ERROR_INPROGRESS;
  if (open_state_ != BEFORE_OPEN)
    return PP_ERROR_FAILED;

  if (!audio_input_callback)
    return PP_ERROR_BADARGUMENT;
  thunk::EnterResourceNoLock<thunk::PPB_AudioConfig_API> enter_config(config,
                                                                      true);
  if (enter_config.failed())
    return PP_ERROR_BADARGUMENT;

  config_ = config;
  audio_input_callback_ = audio_input_callback;
  user_data_ = user_data;
  open_callback_ = callback;

  PpapiHostMsg_AudioInput_Open msg(
      device_id, enter_config.object()->GetSampleRate(),
      enter_config.object()->GetSampleFrameCount());
  Call<PpapiPluginMsg_AudioInput_OpenReply>(
      RENDERER, msg,
      base::Bind(&AudioInputResource::OnPluginMsgOpenReply,
                 base::Unretained(this)));
  return PP_OK_COMPLETIONPENDING;
}

PP_Resource AudioInputResource::GetCurrentConfig() {
  // AddRef for the caller.
  if (config_.get())
    PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool AudioInputResource::StartCapture() {
  if (open_state_ == CLOSED || (open_state_ == BEFORE_OPEN &&
                                !TrackedCallback::IsPending(open_callback_))) {
    return PP_FALSE;
  }
  if (capturing_)
    return PP_TRUE;

  capturing_ = true;
  // Return directly if the audio input device hasn't been opened. Capturing
  // will be started once the open operation is completed.
  if (open_state_ == BEFORE_OPEN)
    return PP_TRUE;

  StartThread();

  Post(RENDERER, PpapiHostMsg_AudioInput_StartOrStop(true));
  return PP_TRUE;
}

PP_Bool AudioInputResource::StopCapture() {
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

  Post(RENDERER, PpapiHostMsg_AudioInput_StartOrStop(false));

  StopThread();
  capturing_ = false;

  return PP_TRUE;
}

void AudioInputResource::Close() {
  if (open_state_ == CLOSED)
    return;

  open_state_ = CLOSED;
  Post(RENDERER, PpapiHostMsg_AudioInput_Close());
  StopThread();

  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->PostAbort();
}

void AudioInputResource::OnPluginMsgEnumerateDevicesReply(
    PP_Resource* devices_resource,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const std::vector<DeviceRefData>& devices) {
  pending_enumerate_devices_ = false;

  // We shouldn't access |devices_resource| if the callback has been called,
  // which is possible if the last plugin reference to this resource has gone
  // away, and the callback has been aborted.
  if (!TrackedCallback::IsPending(callback))
    return;

  if (params.result() == PP_OK) {
    *devices_resource = PPB_DeviceRef_Shared::CreateResourceArray(
        OBJECT_IS_PROXY, pp_instance(), devices);
  }

  callback->Run(params.result());
}

void AudioInputResource::OnPluginMsgOpenReply(
    const ResourceMessageReplyParams& params) {
  if (open_state_ == BEFORE_OPEN && params.result() == PP_OK) {
    IPC::PlatformFileForTransit socket_handle_for_transit =
        IPC::InvalidPlatformFileForTransit();
    params.TakeSocketHandleAtIndex(0, &socket_handle_for_transit);
    base::SyncSocket::Handle socket_handle =
        IPC::PlatformFileForTransitToPlatformFile(socket_handle_for_transit);
    CHECK(socket_handle != base::SyncSocket::kInvalidHandle);

    SerializedHandle serialized_shared_memory_handle =
        params.TakeHandleOfTypeAtIndex(1, SerializedHandle::SHARED_MEMORY);
    CHECK(serialized_shared_memory_handle.IsHandleValid());

    // See the comment in pepper_audio_input_host.cc about how we must call
    // TotalSharedMemorySizeInBytes to get the actual size of the buffer. Here,
    // we must call PacketSizeInBytes to get back the size of the audio buffer,
    // excluding the bytes that audio uses for book-keeping.
    size_t shared_memory_size = media::PacketSizeInBytes(
        serialized_shared_memory_handle.size());

    open_state_ = OPENED;
    SetStreamInfo(serialized_shared_memory_handle.shmem(), shared_memory_size,
                  socket_handle);
  } else {
    capturing_ = false;
  }

  // The callback may have been aborted by Close().
  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->Run(params.result());
}

void AudioInputResource::SetStreamInfo(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::CancelableSyncSocket(socket_handle));
  shared_memory_.reset(new base::SharedMemory(shared_memory_handle, false));
  shared_memory_size_ = shared_memory_size;

  if (!shared_memory_->Map(shared_memory_size_)) {
    PpapiGlobals::Get()->LogWithSource(pp_instance(), PP_LOGLEVEL_WARNING, "",
        "Failed to map shared memory for PPB_AudioInput_Shared.");
  }

  // There is a pending capture request before SetStreamInfo().
  if (capturing_) {
    // Set |capturing_| to false so that the state looks consistent to
    // StartCapture(), which will reset it to true.
    capturing_ = false;
    StartCapture();
  }
}

void AudioInputResource::StartThread() {
  // Don't start the thread unless all our state is set up correctly.
  if (!audio_input_callback_ || !socket_.get() || !capturing_ ||
      !shared_memory_->memory()) {
    return;
  }
  DCHECK(!audio_input_thread_.get());
  audio_input_thread_.reset(new base::DelegateSimpleThread(
      this, "plugin_audio_input_thread"));
  audio_input_thread_->Start();
}

void AudioInputResource::StopThread() {
  // Shut down the socket to escape any hanging |Receive|s.
  if (socket_.get())
    socket_->Shutdown();
  if (audio_input_thread_.get()) {
    audio_input_thread_->Join();
    audio_input_thread_.reset();
  }
}

void AudioInputResource::Run() {
  // The shared memory represents AudioInputBufferParameters and the actual data
  // buffer.
  media::AudioInputBuffer* buffer =
      static_cast<media::AudioInputBuffer*>(shared_memory_->memory());
  uint32_t data_buffer_size =
      shared_memory_size_ - sizeof(media::AudioInputBufferParameters);
  int pending_data;

  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
         pending_data >= 0) {
    // While closing the stream, we may receive buffers whose size is different
    // from |data_buffer_size|.
    CHECK_LE(buffer->params.size, data_buffer_size);
    if (buffer->params.size > 0)
      audio_input_callback_(&buffer->audio[0], buffer->params.size, user_data_);
  }
}

}  // namespace proxy
}  // namespace ppapi
