// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_AUDIO_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_AUDIO_H_

#include <pthread.h>
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/ref_counted.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "media/base/audio_bus.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_audio.h"

namespace ppapi_proxy {

enum PluginAudioState {
  AUDIO_INCOMPLETE = 0,  // StreamCreated not yet invoked
  AUDIO_PENDING,         // Incomplete and app requested StartPlayback
  AUDIO_READY,           // StreamCreated invoked, ready for playback
  AUDIO_PLAYING          // Audio in playback
};

// Implements the untrusted side of the PPB_Audio interface.
// All methods in PluginAudio class will be invoked on the main thread.
// The only exception is AudioThread(), which will invoke the application
// supplied callback to periodically obtain more audio data.
class PluginAudio : public PluginResource {
 public:
  PluginAudio();
  void StreamCreated(NaClSrpcImcDescType socket,
      NaClSrpcImcDescType shm, size_t shm_size);
  void set_state(PluginAudioState state) { state_ = state; }
  void set_callback(PPB_Audio_Callback user_callback, void* user_data) {
      user_callback_ = user_callback;
      user_data_ = user_data;
  }
  PluginAudioState state() { return state_; }
  bool StartAudioThread();
  bool StopAudioThread();
  static void AudioThread(void* self);
  static const PPB_Audio* GetInterface();
  virtual bool InitFromBrowserResource(PP_Resource resource);

 protected:
  virtual ~PluginAudio();

 private:
  PP_Resource resource_;
  NaClSrpcImcDescType socket_;
  NaClSrpcImcDescType shm_;
  size_t shm_size_;
  void *shm_buffer_;
  PluginAudioState state_;
  uintptr_t thread_id_;
  bool thread_active_;
  PPB_Audio_Callback user_callback_;
  void* user_data_;
  // AudioBus for shuttling data across the shared memory.
  scoped_ptr<media::AudioBus> audio_bus_;
  // Internal buffer for client's integer audio data.
  int client_buffer_size_bytes_;
  scoped_array<uint8_t> client_buffer_;

  IMPLEMENT_RESOURCE(PluginAudio);
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginAudio);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_AUDIO_H_
