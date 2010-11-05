// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_AUDIO_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_AUDIO_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/simple_thread.h"
#include "base/sync_socket.h"
#include "ppapi/c/dev/ppb_audio_dev.h"
#include "ppapi/c/dev/ppb_audio_config_dev.h"
#include "ppapi/c/dev/ppb_audio_trusted_dev.h"
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

class Audio : public Resource,
              public PluginDelegate::PlatformAudio::Client,
              public base::DelegateSimpleThread::Delegate {
 public:
  explicit Audio(PluginModule* module);
  virtual ~Audio();

  static const PPB_Audio_Dev* GetInterface();
  static const PPB_AudioTrusted_Dev* GetTrustedInterface();

  bool Init(PluginDelegate* plugin_delegate, PP_Resource config_id,
            PPB_Audio_Callback callback, void* user_data);

  PP_Resource GetCurrentConfiguration() {
    return config_->GetReference();
  }

  bool StartPlayback();

  bool StopPlayback();

  // Resource override.
  virtual Audio* AsAudio();

 private:
  // pepper::PluginDelegate::PlatformAudio::Client implementation.
  virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                             size_t shared_memory_size_,
                             base::SyncSocket::Handle socket);
  // End of pepper::PluginDelegate::PlatformAudio::Client implementation.

  // Audio thread. DelegateSimpleThread::Delegate implementation.
  virtual void Run();
  // End of DelegateSimpleThread::Delegate implementation.

  // True if playing the stream.
  bool playing_;

  // AudioConfig used for creating this Audio object.
  scoped_refptr<AudioConfig> config_;

  // PluginDelegate audio object that we delegate audio IPC through.
  scoped_ptr<PluginDelegate::PlatformAudio> audio_;

  // Socket used to notify us when audio is ready to accept new samples. This
  // pointer is created in StreamCreated().
  scoped_ptr<base::SyncSocket> socket_;

  // Sample buffer in shared memory. This pointer is created in
  // StreamCreated(). The memory is only mapped when the audio thread is
  // created.
  scoped_ptr<base::SharedMemory> shared_memory_;

  // The size of the sample buffer in bytes.
  size_t shared_memory_size_;

  // When the callback is set, this thread is spawned for calling it.
  scoped_ptr<base::DelegateSimpleThread> audio_thread_;

  // Callback to call when audio is ready to accept new samples.
  volatile PPB_Audio_Callback callback_;

  // User data pointer passed verbatim to the callback function.
  void* user_data_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_DEVICE_CONTEXT_AUDIO_H_

