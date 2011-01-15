// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_DEVICE_CONTEXT_AUDIO_H_
#define WEBKIT_PLUGINS_PPAPI_DEVICE_CONTEXT_AUDIO_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/shared_impl/audio_impl.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_AudioConfig_Impl : public Resource {
 public:
  PPB_AudioConfig_Impl(PluginInstance* instance,
                       PP_AudioSampleRate sample_rate,
                       uint32_t sample_frame_count);
  size_t BufferSize();
  static const PPB_AudioConfig* GetInterface();

  PP_AudioSampleRate sample_rate() { return sample_rate_; }
  uint32_t sample_frame_count() { return sample_frame_count_; }

 private:
  // Resource override.
  virtual PPB_AudioConfig_Impl* AsPPB_AudioConfig_Impl();

  PP_AudioSampleRate sample_rate_;
  uint32_t sample_frame_count_;
};

// Some of the backend functionality of this class is implemented by the
// AudioImpl so it can be shared with the proxy.
class PPB_Audio_Impl : public Resource,
                       public pp::shared_impl::AudioImpl,
                       public PluginDelegate::PlatformAudio::Client {
 public:
  explicit PPB_Audio_Impl(PluginInstance* instance);
  virtual ~PPB_Audio_Impl();

  static const PPB_Audio* GetInterface();
  static const PPB_AudioTrusted* GetTrustedInterface();

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
  virtual PPB_Audio_Impl* AsPPB_Audio_Impl();

 private:
  // PluginDelegate::PlatformAudio::Client implementation.
  virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                             size_t shared_memory_size_,
                             base::SyncSocket::Handle socket);

  // AudioConfig used for creating this Audio object.
  scoped_refptr<PPB_AudioConfig_Impl> config_;

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

  DISALLOW_COPY_AND_ASSIGN(PPB_Audio_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_DEVICE_CONTEXT_AUDIO_H_
