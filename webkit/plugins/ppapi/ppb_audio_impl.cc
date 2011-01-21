// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_audio_impl.h"

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "webkit/plugins/ppapi/common.h"

namespace webkit {
namespace ppapi {

namespace {

// PPB_AudioConfig -------------------------------------------------------------

uint32_t RecommendSampleFrameCount(PP_AudioSampleRate sample_rate,
                                   uint32_t requested_sample_frame_count);

PP_Resource CreateStereo16bit(PP_Instance instance_id,
                              PP_AudioSampleRate sample_rate,
                              uint32_t sample_frame_count) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  // TODO(brettw): Currently we don't actually check what the hardware
  // supports, so just allow sample rates of the "guaranteed working" ones.
  if (sample_rate != PP_AUDIOSAMPLERATE_44100 &&
      sample_rate != PP_AUDIOSAMPLERATE_48000)
    return 0;

  // TODO(brettw): Currently we don't actually query to get a value from the
  // hardware, so just validate the range.
  if (RecommendSampleFrameCount(sample_rate, sample_frame_count) !=
      sample_frame_count)
    return 0;

  scoped_refptr<PPB_AudioConfig_Impl> config(
      new PPB_AudioConfig_Impl(instance, sample_rate, sample_frame_count));
  return config->GetReference();
}

uint32_t RecommendSampleFrameCount(PP_AudioSampleRate sample_rate,
                                   uint32_t requested_sample_frame_count) {
  // TODO(brettw) Currently we don't actually query to get a value from the
  // hardware, so we always return the input for in-range values.
  //
  // Danger: this code is duplicated in the audio config proxy.
  if (requested_sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return PP_AUDIOMINSAMPLEFRAMECOUNT;
  if (requested_sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT)
    return PP_AUDIOMAXSAMPLEFRAMECOUNT;
  return requested_sample_frame_count;
}

PP_Bool IsAudioConfig(PP_Resource resource) {
  scoped_refptr<PPB_AudioConfig_Impl> config =
      Resource::GetAs<PPB_AudioConfig_Impl>(resource);
  return BoolToPPBool(!!config);
}

PP_AudioSampleRate GetSampleRate(PP_Resource config_id) {
  scoped_refptr<PPB_AudioConfig_Impl> config =
      Resource::GetAs<PPB_AudioConfig_Impl>(config_id);
  return config ? config->sample_rate() : PP_AUDIOSAMPLERATE_NONE;
}

uint32_t GetSampleFrameCount(PP_Resource config_id) {
  scoped_refptr<PPB_AudioConfig_Impl> config =
      Resource::GetAs<PPB_AudioConfig_Impl>(config_id);
  return config ? config->sample_frame_count() : 0;
}

const PPB_AudioConfig ppb_audioconfig = {
  &CreateStereo16bit,
  &RecommendSampleFrameCount,
  &IsAudioConfig,
  &GetSampleRate,
  &GetSampleFrameCount
};

// PPB_Audio -------------------------------------------------------------------

PP_Resource Create(PP_Instance instance_id, PP_Resource config_id,
                   PPB_Audio_Callback user_callback, void* user_data) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  if (!user_callback)
    return 0;
  scoped_refptr<PPB_Audio_Impl> audio(new PPB_Audio_Impl(instance));
  if (!audio->Init(instance->delegate(), config_id,
                   user_callback, user_data))
    return 0;
  return audio->GetReference();
}

PP_Bool IsAudio(PP_Resource resource) {
  scoped_refptr<PPB_Audio_Impl> audio =
      Resource::GetAs<PPB_Audio_Impl>(resource);
  return BoolToPPBool(!!audio);
}

PP_Resource GetCurrentConfig(PP_Resource audio_id) {
  scoped_refptr<PPB_Audio_Impl> audio =
      Resource::GetAs<PPB_Audio_Impl>(audio_id);
  return audio ? audio->GetCurrentConfig() : 0;
}

PP_Bool StartPlayback(PP_Resource audio_id) {
  scoped_refptr<PPB_Audio_Impl> audio =
      Resource::GetAs<PPB_Audio_Impl>(audio_id);
  return audio ? BoolToPPBool(audio->StartPlayback()) : PP_FALSE;
}

PP_Bool StopPlayback(PP_Resource audio_id) {
  scoped_refptr<PPB_Audio_Impl> audio =
      Resource::GetAs<PPB_Audio_Impl>(audio_id);
  return audio ? BoolToPPBool(audio->StopPlayback()) : PP_FALSE;
}

const PPB_Audio ppb_audio = {
  &Create,
  &IsAudio,
  &GetCurrentConfig,
  &StartPlayback,
  &StopPlayback,
};

// PPB_AudioTrusted ------------------------------------------------------------

PP_Resource CreateTrusted(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  scoped_refptr<PPB_Audio_Impl> audio(new PPB_Audio_Impl(instance));
  return audio->GetReference();
}

int32_t Open(PP_Resource audio_id,
             PP_Resource config_id,
             PP_CompletionCallback created) {
  scoped_refptr<PPB_Audio_Impl> audio =
      Resource::GetAs<PPB_Audio_Impl>(audio_id);
  if (!audio)
    return PP_ERROR_BADRESOURCE;
  if (!created.func)
    return PP_ERROR_BADARGUMENT;
  return audio->Open(audio->instance()->delegate(), config_id, created);
}

int32_t GetSyncSocket(PP_Resource audio_id, int* sync_socket) {
  scoped_refptr<PPB_Audio_Impl> audio =
      Resource::GetAs<PPB_Audio_Impl>(audio_id);
  if (audio)
    return audio->GetSyncSocket(sync_socket);
  return PP_ERROR_BADRESOURCE;
}

int32_t GetSharedMemory(PP_Resource audio_id,
                        int* shm_handle,
                        uint32_t* shm_size) {
  scoped_refptr<PPB_Audio_Impl> audio =
      Resource::GetAs<PPB_Audio_Impl>(audio_id);
  if (audio)
    return audio->GetSharedMemory(shm_handle, shm_size);
  return PP_ERROR_BADRESOURCE;
}

const PPB_AudioTrusted ppb_audiotrusted = {
  &CreateTrusted,
  &Open,
  &GetSyncSocket,
  &GetSharedMemory,
};

}  // namespace

// PPB_AudioConfig_Impl --------------------------------------------------------

PPB_AudioConfig_Impl::PPB_AudioConfig_Impl(
    PluginInstance* instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count)
    : Resource(instance),
      sample_rate_(sample_rate),
      sample_frame_count_(sample_frame_count) {
}

const PPB_AudioConfig* PPB_AudioConfig_Impl::GetInterface() {
  return &ppb_audioconfig;
}

size_t PPB_AudioConfig_Impl::BufferSize() {
  // TODO(audio): as more options become available, we'll need to
  // have additional code here to correctly calculate the size in
  // bytes of an audio buffer.  For now, only support two channel
  // int16_t sample buffers.
  const int kChannels = 2;
  const int kSizeOfSample = sizeof(int16_t);
  return static_cast<size_t>(sample_frame_count_ * kSizeOfSample * kChannels);
}

PPB_AudioConfig_Impl* PPB_AudioConfig_Impl::AsPPB_AudioConfig_Impl() {
  return this;
}

// PPB_Audio_Impl --------------------------------------------------------------

PPB_Audio_Impl::PPB_Audio_Impl(PluginInstance* instance)
    : Resource(instance),
      audio_(NULL),
      create_callback_pending_(false) {
  create_callback_ = PP_MakeCompletionCallback(NULL, NULL);
}

PPB_Audio_Impl::~PPB_Audio_Impl() {
  // Calling ShutDown() makes sure StreamCreated cannot be called anymore.
  if (audio_) {
    audio_->ShutDown();
    audio_ = NULL;
  }

  // If the completion callback hasn't fired yet, do so here
  // with an error condition.
  if (create_callback_pending_) {
    PP_RunCompletionCallback(&create_callback_, PP_ERROR_ABORTED);
    create_callback_pending_ = false;
  }
}

const PPB_Audio* PPB_Audio_Impl::GetInterface() {
  return &ppb_audio;
}

const PPB_AudioTrusted* PPB_Audio_Impl::GetTrustedInterface() {
  return &ppb_audiotrusted;
}

PPB_Audio_Impl* PPB_Audio_Impl::AsPPB_Audio_Impl() {
  return this;
}

bool PPB_Audio_Impl::Init(PluginDelegate* plugin_delegate,
                          PP_Resource config_id,
                          PPB_Audio_Callback callback, void* user_data) {
  CHECK(!audio_);
  config_ = Resource::GetAs<PPB_AudioConfig_Impl>(config_id);
  if (!config_)
    return false;
  SetCallback(callback, user_data);

  // When the stream is created, we'll get called back on StreamCreated().
  audio_ = plugin_delegate->CreateAudio(config_->sample_rate(),
                                        config_->sample_frame_count(),
                                        this);
  return audio_ != NULL;
}

PP_Resource PPB_Audio_Impl::GetCurrentConfig() {
  return config_->GetReference();
}

bool PPB_Audio_Impl::StartPlayback() {
  if (!audio_)
    return false;
  if (playing())
    return true;
  SetStartPlaybackState();
  return audio_->StartPlayback();
}

bool PPB_Audio_Impl::StopPlayback() {
  if (!audio_)
    return false;
  if (!playing())
    return true;
  if (!audio_->StopPlayback())
    return false;
  SetStopPlaybackState();
  return true;
}

int32_t PPB_Audio_Impl::Open(PluginDelegate* plugin_delegate,
                             PP_Resource config_id,
                             PP_CompletionCallback create_callback) {
  DCHECK(!audio_);
  config_ = Resource::GetAs<PPB_AudioConfig_Impl>(config_id);
  if (!config_)
    return PP_ERROR_BADRESOURCE;

  // When the stream is created, we'll get called back on StreamCreated().
  audio_ = plugin_delegate->CreateAudio(config_->sample_rate(),
                                        config_->sample_frame_count(),
                                        this);
  if (!audio_)
    return PP_ERROR_FAILED;

  // At this point, we are guaranteeing ownership of the completion
  // callback.  Audio promises to fire the completion callback
  // once and only once.
  create_callback_ = create_callback;
  create_callback_pending_ = true;
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_Audio_Impl::GetSyncSocket(int* sync_socket) {
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

int32_t PPB_Audio_Impl::GetSharedMemory(int* shm_handle, uint32_t* shm_size) {
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

void PPB_Audio_Impl::StreamCreated(
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
    SetStreamInfo(shared_memory_handle, shared_memory_size, socket_handle);
  }
}

}  // namespace ppapi
}  // namespace webkit

