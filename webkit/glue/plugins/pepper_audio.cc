// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_audio.h"

#include "base/logging.h"
#include "ppapi/c/dev/ppb_audio_dev.h"
#include "ppapi/c/dev/ppb_audio_trusted_dev.h"
#include "webkit/glue/plugins/pepper_common.h"

namespace pepper {

namespace {

// PPB_AudioConfig -------------------------------------------------------------

uint32_t RecommendSampleFrameCount(uint32_t requested_sample_frame_count);

PP_Resource CreateStereo16bit(PP_Module module_id,
                              PP_AudioSampleRate_Dev sample_rate,
                              uint32_t sample_frame_count) {
  PluginModule* module = ResourceTracker::Get()->GetModule(module_id);
  if (!module)
    return 0;

  // TODO(brettw): Currently we don't actually check what the hardware
  // supports, so just allow sample rates of the "guaranteed working" ones.
  if (sample_rate != PP_AUDIOSAMPLERATE_44100 &&
      sample_rate != PP_AUDIOSAMPLERATE_48000)
    return 0;

  // TODO(brettw): Currently we don't actually query to get a value from the
  // hardware, so just validate the range.
  if (RecommendSampleFrameCount(sample_frame_count) != sample_frame_count)
    return 0;

  scoped_refptr<AudioConfig> config(new AudioConfig(module,
                                                    sample_rate,
                                                    sample_frame_count));
  return config->GetReference();
}

uint32_t RecommendSampleFrameCount(uint32_t requested_sample_frame_count) {
  // TODO(brettw) Currently we don't actually query to get a value from the
  // hardware, so we always return the input for in-range values.
  if (requested_sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return PP_AUDIOMINSAMPLEFRAMECOUNT;
  if (requested_sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT)
    return PP_AUDIOMAXSAMPLEFRAMECOUNT;
  return requested_sample_frame_count;
}

PP_Bool IsAudioConfig(PP_Resource resource) {
  scoped_refptr<AudioConfig> config = Resource::GetAs<AudioConfig>(resource);
  return BoolToPPBool(!!config);
}

PP_AudioSampleRate_Dev GetSampleRate(PP_Resource config_id) {
  scoped_refptr<AudioConfig> config = Resource::GetAs<AudioConfig>(config_id);
  return config ? config->sample_rate() : PP_AUDIOSAMPLERATE_NONE;
}

uint32_t GetSampleFrameCount(PP_Resource config_id) {
  scoped_refptr<AudioConfig> config = Resource::GetAs<AudioConfig>(config_id);
  return config ? config->sample_frame_count() : 0;
}

const PPB_AudioConfig_Dev ppb_audioconfig = {
  &CreateStereo16bit,
  &RecommendSampleFrameCount,
  &IsAudioConfig,
  &GetSampleRate,
  &GetSampleFrameCount
};

// PPB_Audio -------------------------------------------------------------------

PP_Resource Create(PP_Instance instance_id, PP_Resource config_id,
                   PPB_Audio_Callback callback, void* user_data) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;
  // TODO(neb): Require callback to be present for untrusted plugins.
  scoped_refptr<Audio> audio(new Audio(instance->module()));
  if (!audio->Init(instance->delegate(), config_id, callback, user_data))
    return 0;
  return audio->GetReference();
}

PP_Bool IsAudio(PP_Resource resource) {
  scoped_refptr<Audio> audio = Resource::GetAs<Audio>(resource);
  return BoolToPPBool(!!audio);
}

PP_Resource GetCurrentConfiguration(PP_Resource audio_id) {
  scoped_refptr<Audio> audio = Resource::GetAs<Audio>(audio_id);
  return audio ? audio->GetCurrentConfiguration() : 0;
}

PP_Bool StartPlayback(PP_Resource audio_id) {
  scoped_refptr<Audio> audio = Resource::GetAs<Audio>(audio_id);
  return audio ? BoolToPPBool(audio->StartPlayback()) : PP_FALSE;
}

PP_Bool StopPlayback(PP_Resource audio_id) {
  scoped_refptr<Audio> audio = Resource::GetAs<Audio>(audio_id);
  return audio ? BoolToPPBool(audio->StopPlayback()) : PP_FALSE;
}

const PPB_Audio_Dev ppb_audio = {
  &Create,
  &IsAudio,
  &GetCurrentConfiguration,
  &StartPlayback,
  &StopPlayback,
};

// PPB_AudioTrusted ------------------------------------------------------------

PP_Resource GetBuffer(PP_Resource audio_id) {
  // TODO(neb): Implement me!
  return 0;
}

int GetOSDescriptor(PP_Resource audio_id) {
  // TODO(neb): Implement me!
  return -1;
}

const PPB_AudioTrusted_Dev ppb_audiotrusted = {
  &GetBuffer,
  &GetOSDescriptor
};

}  // namespace

// AudioConfig -----------------------------------------------------------------

AudioConfig::AudioConfig(PluginModule* module,
                         PP_AudioSampleRate_Dev sample_rate,
                         uint32_t sample_frame_count)
    : Resource(module),
      sample_rate_(sample_rate),
      sample_frame_count_(sample_frame_count) {
}

const PPB_AudioConfig_Dev* AudioConfig::GetInterface() {
  return &ppb_audioconfig;
}

size_t AudioConfig::BufferSize() {
  // TODO(audio): as more options become available, we'll need to
  // have additional code here to correctly calculate the size in
  // bytes of an audio buffer.  For now, only support two channel
  // int16_t sample buffers.
  const int kChannels = 2;
  const int kSizeOfSample = sizeof(int16_t);
  return static_cast<size_t>(sample_frame_count_ * kSizeOfSample * kChannels);
}

AudioConfig* AudioConfig::AsAudioConfig() {
  return this;
}

// Audio -----------------------------------------------------------------------

Audio::Audio(PluginModule* module)
    : Resource(module),
      playing_(false),
      socket_(NULL),
      shared_memory_(NULL),
      shared_memory_size_(0),
      callback_(NULL),
      user_data_(NULL) {
}

Audio::~Audio() {
  // Calling ShutDown() makes sure StreamCreated cannot be called anymore.
  audio_->ShutDown();
  // Closing the socket causes the thread to exit - wait for it.
  socket_->Close();
  if (audio_thread_.get()) {
    audio_thread_->Join();
    audio_thread_.reset();
  }
  // Shared memory destructor will unmap the memory automatically.
}

const PPB_Audio_Dev* Audio::GetInterface() {
  return &ppb_audio;
}

const PPB_AudioTrusted_Dev* Audio::GetTrustedInterface() {
  return &ppb_audiotrusted;
}

Audio* Audio::AsAudio() {
  return this;
}

bool Audio::Init(PluginDelegate* plugin_delegate, PP_Resource config_id,
                 PPB_Audio_Callback callback, void* user_data) {
  CHECK(!audio_.get());
  config_ = Resource::GetAs<AudioConfig>(config_id);
  if (!config_)
    return false;
  callback_ = callback;
  user_data_ = user_data;
  // When the stream is created, we'll get called back in StreamCreated().
  audio_.reset(plugin_delegate->CreateAudio(config_->sample_rate(),
                                            config_->sample_frame_count(),
                                            this));
  return audio_.get() != NULL;
}

bool Audio::StartPlayback() {
  if (playing_)
    return true;

  CHECK(!audio_thread_.get());
  if (callback_ && socket_.get()) {
    audio_thread_.reset(new base::DelegateSimpleThread(this,
                                                       "plugin_audio_thread"));
    audio_thread_->Start();
  }
  playing_ = true;
  return audio_->StartPlayback();
}

bool Audio::StopPlayback() {
  if (!playing_)
    return true;

  if (!audio_->StopPlayback())
    return false;

  if (audio_thread_.get()) {
    audio_thread_->Join();
    audio_thread_.reset();
  }
  playing_ = false;
  return true;
}

void Audio::StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                          size_t shared_memory_size,
                          base::SyncSocket::Handle socket_handle) {
  socket_.reset(new base::SyncSocket(socket_handle));
  shared_memory_.reset(new base::SharedMemory(shared_memory_handle, false));
  shared_memory_size_ = shared_memory_size;

  if (callback_) {
    shared_memory_->Map(shared_memory_size_);
    // In common case StartPlayback() was called before StreamCreated().
    if (playing_) {
      audio_thread_.reset(new base::DelegateSimpleThread(this,
          "plugin_audio_thread"));
      audio_thread_->Start();
    }
  }
}

void Audio::Run() {
  int pending_data;
  void* buffer = shared_memory_->memory();
  size_t buffer_size_in_bytes = config_->BufferSize();

  while (sizeof(pending_data) ==
      socket_->Receive(&pending_data, sizeof(pending_data)) &&
      pending_data >= 0) {
    // Exit the thread on pause.
    if (pending_data < 0)
      return;
    callback_(buffer, buffer_size_in_bytes, user_data_);
  }
}

}  // namespace pepper

