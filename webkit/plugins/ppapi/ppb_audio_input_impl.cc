// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_audio_input_impl.h"

#include "base/logging.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/trusted/ppb_audio_input_trusted_dev.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"
#include "ppapi/thunk/thunk.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::PpapiGlobals;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_AudioInput_API;
using ppapi::thunk::PPB_AudioConfig_API;

namespace webkit {
namespace ppapi {

// PPB_AudioInput_Impl ---------------------------------------------------------

PPB_AudioInput_Impl::PPB_AudioInput_Impl(PP_Instance instance)
    : Resource(instance),
      audio_input_(NULL) {
}

PPB_AudioInput_Impl::~PPB_AudioInput_Impl() {
  // Calling ShutDown() makes sure StreamCreated cannot be called anymore and
  // releases the audio data associated with the pointer. Note however, that
  // until ShutDown returns, StreamCreated may still be called. This will be
  // OK since we'll just immediately clean up the data it stored later in this
  // destructor.
  if (audio_input_) {
    audio_input_->ShutDown();
    audio_input_ = NULL;
  }
}

// static
PP_Resource PPB_AudioInput_Impl::Create(PP_Instance instance,
                                   PP_Resource config,
                                   PPB_AudioInput_Callback audio_input_callback,
                                   void* user_data) {
  scoped_refptr<PPB_AudioInput_Impl>
      audio_input(new PPB_AudioInput_Impl(instance));
  if (!audio_input->Init(config, audio_input_callback, user_data))
    return 0;
  return audio_input->GetReference();
}

PPB_AudioInput_API* PPB_AudioInput_Impl::AsPPB_AudioInput_API() {
  return this;
}

bool PPB_AudioInput_Impl::Init(PP_Resource config,
                               PPB_AudioInput_Callback callback,
                               void* user_data) {
  // Validate the config and keep a reference to it.
  EnterResourceNoLock<PPB_AudioConfig_API> enter(config, true);
  if (enter.failed())
    return false;
  config_ = config;

  if (!callback)
    return false;
  SetCallback(callback, user_data);

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return false;

  // When the stream is created, we'll get called back on StreamCreated().
  CHECK(!audio_input_);
  audio_input_ = plugin_delegate->CreateAudioInput(
      enter.object()->GetSampleRate(),
      enter.object()->GetSampleFrameCount(),
      this);
  return audio_input_ != NULL;
}

PP_Resource PPB_AudioInput_Impl::GetCurrentConfig() {
  // AddRef on behalf of caller, while keeping a ref for ourselves.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool PPB_AudioInput_Impl::StartCapture() {
  if (!audio_input_)
    return PP_FALSE;
  if (capturing())
    return PP_TRUE;
  SetStartCaptureState();
  return BoolToPPBool(audio_input_->StartCapture());
}

PP_Bool PPB_AudioInput_Impl::StopCapture() {
  if (!audio_input_)
    return PP_FALSE;
  if (!capturing())
    return PP_TRUE;
  if (!audio_input_->StopCapture())
    return PP_FALSE;
  SetStopCaptureState();
  return PP_TRUE;
}

int32_t PPB_AudioInput_Impl::OpenTrusted(PP_Resource config,
    PP_CompletionCallback create_callback) {
  // Validate the config and keep a reference to it.
  EnterResourceNoLock<PPB_AudioConfig_API> enter(config, true);
  if (enter.failed())
    return PP_ERROR_FAILED;
  config_ = config;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  // When the stream is created, we'll get called back on StreamCreated().
  DCHECK(!audio_input_);
  audio_input_ = plugin_delegate->CreateAudioInput(
      enter.object()->GetSampleRate(),
      enter.object()->GetSampleFrameCount(),
      this);

  if (!audio_input_)
    return PP_ERROR_FAILED;

  // At this point, we are guaranteeing ownership of the completion
  // callback.  Audio promises to fire the completion callback
  // once and only once.
  SetCallbackInfo(true, create_callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_AudioInput_Impl::GetSyncSocket(int* sync_socket) {
  return GetSyncSocketImpl(sync_socket);
}

int32_t PPB_AudioInput_Impl::GetSharedMemory(int* shm_handle,
                                             uint32_t* shm_size) {
  return GetSharedMemoryImpl(shm_handle, shm_size);
}

void PPB_AudioInput_Impl::OnSetStreamInfo(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  SetStreamInfo(shared_memory_handle, shared_memory_size, socket_handle);
}

}  // namespace ppapi
}  // namespace webkit
