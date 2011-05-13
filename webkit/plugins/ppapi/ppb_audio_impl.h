// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/shared_impl/audio_config_impl.h"
#include "ppapi/shared_impl/audio_impl.h"
#include "ppapi/thunk/ppb_audio_trusted_api.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

// The implementation is actually in AudioConfigImpl.
class PPB_AudioConfig_Impl : public Resource,
                             public ::ppapi::AudioConfigImpl {
 public:
  // Note that you must call Init (on AudioConfigImpl) before using this class.
  PPB_AudioConfig_Impl(PluginInstance* instance);
  ~PPB_AudioConfig_Impl();

  // ResourceObjectBase overrides.
  virtual ::ppapi::thunk::PPB_AudioConfig_API* AsAudioConfig_API() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_AudioConfig_Impl);
};

// Some of the backend functionality of this class is implemented by the
// AudioImpl so it can be shared with the proxy.
class PPB_Audio_Impl : public Resource,
                       public ::ppapi::AudioImpl,
                       public ::ppapi::thunk::PPB_AudioTrusted_API,
                       public PluginDelegate::PlatformAudio::Client {
 public:
  // After creation, either call Init (for non-trusted init) or OpenTrusted
  // (for trusted init).
  explicit PPB_Audio_Impl(PluginInstance* instance);
  virtual ~PPB_Audio_Impl();

  // Initialization function for non-trusted init.
  bool Init(PP_Resource config_id,
            PPB_Audio_Callback user_callback, void* user_data);

  // ResourceObjectBase overrides.
  virtual ::ppapi::thunk::PPB_Audio_API* AsAudio_API();
  virtual ::ppapi::thunk::PPB_AudioTrusted_API* AsAudioTrusted_API();

  // PPB_Audio_API implementation.
  virtual PP_Resource GetCurrentConfig() OVERRIDE;
  virtual PP_Bool StartPlayback() OVERRIDE;
  virtual PP_Bool StopPlayback() OVERRIDE;

  // PPB_AudioTrusted_API implementation.
  virtual int32_t OpenTrusted(PP_Resource config_id,
                              PP_CompletionCallback create_callback) OVERRIDE;
  virtual int32_t GetSyncSocket(int* sync_socket) OVERRIDE;
  virtual int32_t GetSharedMemory(int* shm_handle, uint32_t* shm_size) OVERRIDE;

 private:
  // PluginDelegate::PlatformAudio::Client implementation.
  virtual void StreamCreated(base::SharedMemoryHandle shared_memory_handle,
                             size_t shared_memory_size_,
                             base::SyncSocket::Handle socket);

  // AudioConfig used for creating this Audio object. We own a ref.
  PP_Resource config_id_;

  // PluginDelegate audio object that we delegate audio IPC through. We don't
  // own this pointer but are responsible for calling Shutdown on it.
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

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_AUDIO_IMPL_H_
