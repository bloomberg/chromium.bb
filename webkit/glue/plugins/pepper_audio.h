// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_AUDIO_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_AUDIO_H_

#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ppapi/c/dev/ppb_audio_dev.h"
#include "ppapi/c/dev/ppb_audio_config_dev.h"
#include "ppapi/c/dev/ppb_audio_trusted_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/shared_impl/audio_impl.h"
#include "webkit/glue/plugins/pepper_plugin_delegate.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_plugin_module.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

class PluginInstance;
class PluginModule;

class AudioConfig : public Resource {
 public:
  AudioConfig(PluginModule* module,
              PP_AudioSampleRate_Dev sample_rate,
              uint32_t sample_frame_count);
  size_t BufferSize();
  static const PPB_AudioConfig_Dev* GetInterface();

  PP_AudioSampleRate_Dev sample_rate() { return sample_rate_; }
  uint32_t sample_frame_count() { return sample_frame_count_; }

 private:
  // Resource override.
  virtual AudioConfig* AsAudioConfig();

  PP_AudioSampleRate_Dev sample_rate_;
  uint32_t sample_frame_count_;
};

// Some of the backend functionality of this class is implemented by the
// AudioImpl so it can be shared with the proxy.
class Audio : public Resource,
              public pp::shared_impl::AudioImpl,
              public PluginDelegate::PlatformAudio::Client {
 public:
  explicit Audio(PluginModule* module, PP_Instance instance_id);
  virtual ~Audio();

  static const PPB_Audio_Dev* GetInterface();
  static const PPB_AudioTrusted_Dev* GetTrustedInterface();

  PP_Instance pp_instance() {
    return pp_instance_;
  }

  // PPB_Audio implementation.
  bool Init(PluginDelegate* plugin_delegate,
            PP_Resource config_id,
            PPB_Audio_Callback user_callback, void* user_data);
  PP_Resource GetCurrentConfig();
  bool StartPlayback();
  bool StopPlayback();

  // PPB_Audio_Trusted implementation.
  int32_t Open(PluginDelegate* plugin_delegate,
               PP_Resource config_id,
               PP_CompletionCallback create_callback);
  int32_t GetSyncSocket(int* sync_socket);
  int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size);

  // Resource override.
  virtual Audio* AsAudio();

 private:
  // pepper::PluginDelegate::PlatformAudio::Client implementation.
  virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                             size_t shared_memory_size_,
                             base::SyncSocket::Handle socket);

  // AudioConfig used for creating this Audio object.
  scoped_refptr<AudioConfig> config_;

  // Plugin instance that owns this audio object.
  PP_Instance pp_instance_;

  // PluginDelegate audio object that we delegate audio IPC through.
  PluginDelegate::PlatformAudio* audio_;

  // Is a create callback pending to fire?
  bool create_callback_pending_;

  // Trusted callback invoked from StreamCreated.
  PP_CompletionCallback create_callback_;

  // When a create callback is being issued, these will save the info for
  // querying from the callback. The proxy uses this to get the handles to the
  // other process instead of mapping them in the renderer. These will be
  // invalid all other times.
  scoped_ptr<base::SharedMemory> shared_memory_for_create_callback_;
  size_t shared_memory_size_for_create_callback_;
  scoped_ptr<base::SyncSocket> socket_for_create_callback_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_AUDIO_H_
