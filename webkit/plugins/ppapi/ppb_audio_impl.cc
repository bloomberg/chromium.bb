// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_audio_impl.h"

#include "base/logging.h"
#include "media/audio/audio_output_controller.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_audio_config_api.h"
#include "ppapi/thunk/thunk.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::PpapiGlobals;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Audio_API;
using ppapi::thunk::PPB_AudioConfig_API;
using ppapi::TrackedCallback;

namespace webkit {
namespace ppapi {

// PPB_Audio_Impl --------------------------------------------------------------

PPB_Audio_Impl::PPB_Audio_Impl(PP_Instance instance)
    : Resource(::ppapi::OBJECT_IS_IMPL, instance),
      audio_(NULL),
      sample_frame_count_(0) {
}

PPB_Audio_Impl::~PPB_Audio_Impl() {
  // Calling ShutDown() makes sure StreamCreated cannot be called anymore and
  // releases the audio data associated with the pointer. Note however, that
  // until ShutDown returns, StreamCreated may still be called. This will be
  // OK since we'll just immediately clean up the data it stored later in this
  // destructor.
  if (audio_) {
    audio_->ShutDown();
    audio_ = NULL;
  }
}

// static
PP_Resource PPB_Audio_Impl::Create(PP_Instance instance,
                                   PP_Resource config,
                                   PPB_Audio_Callback audio_callback,
                                   void* user_data) {
  scoped_refptr<PPB_Audio_Impl> audio(new PPB_Audio_Impl(instance));
  if (!audio->Init(config, audio_callback, user_data))
    return 0;
  return audio->GetReference();
}

PPB_Audio_API* PPB_Audio_Impl::AsPPB_Audio_API() {
  return this;
}

bool PPB_Audio_Impl::Init(PP_Resource config,
                          PPB_Audio_Callback callback, void* user_data) {
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
  CHECK(!audio_);
  audio_ = plugin_delegate->CreateAudioOutput(
      enter.object()->GetSampleRate(), enter.object()->GetSampleFrameCount(),
      this);
  sample_frame_count_ = enter.object()->GetSampleFrameCount();
  return audio_ != NULL;
}

PP_Resource PPB_Audio_Impl::GetCurrentConfig() {
  // AddRef on behalf of caller, while keeping a ref for ourselves.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(config_);
  return config_;
}

PP_Bool PPB_Audio_Impl::StartPlayback() {
  if (!audio_)
    return PP_FALSE;
  if (playing())
    return PP_TRUE;
  SetStartPlaybackState();
  return BoolToPPBool(audio_->StartPlayback());
}

PP_Bool PPB_Audio_Impl::StopPlayback() {
  if (!audio_)
    return PP_FALSE;
  if (!playing())
    return PP_TRUE;
  if (!audio_->StopPlayback())
    return PP_FALSE;
  SetStopPlaybackState();
  return PP_TRUE;
}

int32_t PPB_Audio_Impl::OpenTrusted(
    PP_Resource config,
    scoped_refptr<TrackedCallback> create_callback) {
  // Validate the config and keep a reference to it.
  EnterResourceNoLock<PPB_AudioConfig_API> enter(config, true);
  if (enter.failed())
    return PP_ERROR_FAILED;
  config_ = config;

  PluginDelegate* plugin_delegate = ResourceHelper::GetPluginDelegate(this);
  if (!plugin_delegate)
    return PP_ERROR_FAILED;

  // When the stream is created, we'll get called back on StreamCreated().
  DCHECK(!audio_);
  audio_ = plugin_delegate->CreateAudioOutput(
      enter.object()->GetSampleRate(), enter.object()->GetSampleFrameCount(),
      this);
  if (!audio_)
    return PP_ERROR_FAILED;

  // At this point, we are guaranteeing ownership of the completion
  // callback.  Audio promises to fire the completion callback
  // once and only once.
  SetCreateCallback(create_callback);

  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_Audio_Impl::GetSyncSocket(int* sync_socket) {
  return GetSyncSocketImpl(sync_socket);
}

int32_t PPB_Audio_Impl::GetSharedMemory(int* shm_handle,
                                        uint32_t* shm_size) {
  return GetSharedMemoryImpl(shm_handle, shm_size);
}

void PPB_Audio_Impl::OnSetStreamInfo(
    base::SharedMemoryHandle shared_memory_handle,
    size_t shared_memory_size,
    base::SyncSocket::Handle socket_handle) {
  SetStreamInfo(pp_instance(), shared_memory_handle, shared_memory_size,
                socket_handle, sample_frame_count_);
}

}  // namespace ppapi
}  // namespace webkit
