// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_audio.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "ppapi/c/dev/ppb_audio_config_dev.h"
#include "ppapi/c/dev/ppb_audio_dev.h"
#include "ppapi/cpp/common.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {
namespace {

enum AudioProxyState {
  AUDIO_INCOMPLETE = 0,  // StreamCreated not yet invoked
  AUDIO_PENDING,         // Incomplete and app requested StartPlayback
  AUDIO_READY,           // StreamCreated invoked, ready for playback
  AUDIO_PLAYING          // Audio in playback
};

// All methods in AudioProxy class will be invoked on the main thread.
// The only exception is AudioThread(), which will invoke the application
// supplied callback to periodically obtain more audio data.
class AudioProxy {
 public:
  AudioProxy(PP_Resource audioResource,
      PPB_Audio_Callback user_callback, void* user_data);
  virtual ~AudioProxy();
  void StreamCreated(NaClSrpcImcDescType socket,
      NaClSrpcImcDescType shm, size_t shm_size);
  void set_state(AudioProxyState state) { state_ = state; }
  AudioProxyState state() { return state_; }
  bool StartAudioThread();
  bool StopAudioThread();
  static void* AudioThread(void* self);
 private:
  const PP_Resource audio_resource_;
  const PPB_Audio_Dev* audio_interface_;
  NaClSrpcImcDescType socket_;
  NaClSrpcImcDescType shm_;
  size_t shm_size_;
  void *shm_buffer_;
  AudioProxyState state_;
  pthread_t thread_id_;
  PPB_Audio_Callback user_callback_;
  void* user_data_;
};

// TODO(audio): support for multiple simultaneous audio devices
// Currently, we only support _one_ audio device per untrusted
// process.  This isn't as bad as it may sound; higher level
// audio APIs can mix multiple audio sources into this single stream.
PP_Resource audioResource = kInvalidResourceId;
AudioProxy* audioInstance = NULL;

// socket_read() implemented here as a NACL_SYSCALL to avoid
// conflicts with projects that attempt to wrap the read() call
// using linker command line option "--wrap read"
ssize_t socket_read(int desc, void *buf, size_t count) {
  ssize_t retval = NACL_SYSCALL(read)(desc, buf, count);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}

bool BindAudioResource(PP_Resource resource,
                       AudioProxy *instance) {
  // TODO(audio): support for multiple simultanious audio devices
  if (NULL != audioInstance) {
    return false;
  }
  audioResource = resource;
  audioInstance = instance;
  return true;
}

bool UnBindAudioResource(PP_Resource resource) {
  // TODO(audio): support for multiple simultanious audio devices
  if (NULL == audioInstance) {
    return false;
  }
  if (audioResource != resource) {
    return false;
  }
  audioResource = kInvalidResourceId;
  audioInstance = NULL;
  return true;
}

AudioProxy* GetBoundAudio(PP_Resource resource) {
  // TODO(audio): support for multiple simultanious audio devices
  if (audioResource != resource) {
    return NULL;
  }
  return audioInstance;
}

AudioProxy* GetCurrentAudio() {
  // TODO(audio): this will go away when we support multiple
  // active instances of untrusted audio.
  return audioInstance;
}

AudioProxy::AudioProxy(PP_Resource resource,
                       PPB_Audio_Callback user_callback, void* user_data) :
    audio_resource_(resource),
    socket_(-1),
    shm_(-1),
    shm_size_(0),
    shm_buffer_(NULL),
    state_(AUDIO_INCOMPLETE),
    thread_id_(),
    user_callback_(user_callback),
    user_data_(user_data) {
  audio_interface_ = static_cast<const PPB_Audio_Dev*>
      (PluginAudio::GetInterface());
}

AudioProxy::~AudioProxy() {
  // Stop playback should terminate the audio thread, if
  // one is currently running.
  audio_interface_->StopPlayback(audio_resource_);
  // Unmap the shared memory buffer, if present.
  if (shm_buffer_) {
    munmap(shm_buffer_, shm_size_);
    shm_buffer_ = NULL;
    shm_size_ = 0;
  }
  // Close the handles.
  if (shm_ != -1) {
    close(shm_);
    shm_ = -1;
  }
  if (socket_ != -1) {
    close(socket_);
    socket_ = -1;
  }
  UnBindAudioResource(audio_resource_);
}

void* AudioProxy::AudioThread(void* self) {
  AudioProxy* audio = static_cast<AudioProxy*>(self);
  while (true) {
    int32_t an_int32;
    ssize_t r;
    // invoke user callback, get next buffer of audio data
    audio->user_callback_(
        audio->shm_buffer_,
        audio->shm_size_,
        audio->user_data_);
    // block on socket read
    r = socket_read(
        audio->socket_,
        &an_int32, sizeof(an_int32));
    if (sizeof(an_int32) != r)
      break;
  }
  return NULL;
}

void AudioProxy::StreamCreated(NaClSrpcImcDescType socket,
      NaClSrpcImcDescType shm, size_t shm_size) {
  socket_ = socket;
  shm_ = shm;
  shm_size_ = shm_size;
  shm_buffer_ = mmap(NULL,
                     shm_size,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     shm,
                     0);
  if (MAP_FAILED != shm_buffer_) {
    if (state() == AUDIO_PENDING) {
      StartAudioThread();
    } else {
      set_state(AUDIO_READY);
    }
  } else {
    shm_buffer_ = NULL;
  }
}

bool AudioProxy::StartAudioThread() {
  int ret = pthread_create(&thread_id_, NULL, AudioThread, this);
  if (0 == ret) {
    set_state(AUDIO_PLAYING);
    return true;
  }
  return false;
}

bool AudioProxy::StopAudioThread() {
  int ret = pthread_join(thread_id_, NULL);
  if (0 == ret) {
    set_state(AUDIO_READY);
    return true;
  }
  return false;
}

// Start of untrusted PPB_Audio functions

PP_Resource Create(PP_Instance instance,
                   PP_Resource config,
                   PPB_Audio_Callback user_callback,
                   void* user_data) {
  PP_Resource audioResource;
  NaClSrpcError retval = NACL_SRPC_RESULT_OK;
  NaClSrpcChannel* channel = NULL;
  AudioProxy* audio = NULL;
  // TODO(audio): remove check below when & if we support
  // multiple simultanious audio devices from same untrusted instance
  if (NULL != GetCurrentAudio()) {
    return kInvalidResourceId;
  }
  // Proxy to browser Create, get audio PP_Resource
  channel = ppapi_proxy::GetMainSrpcChannel();
  retval = PpbAudioDevRpcClient::PPB_Audio_Dev_Create(
      channel,
      instance,
      config,
      &audioResource);
  if (NACL_SRPC_RESULT_OK != retval) {
    return kInvalidResourceId;
  }
  if (kInvalidResourceId == audioResource) {
    return kInvalidResourceId;
  }
  // Create an AudioProxy
  audio = new AudioProxy(audioResource, user_callback, user_data);
  if (NULL == audio) {
    return kInvalidResourceId;
  }
  // Bind AudioProxy to audio PP_Resource
  if (!BindAudioResource(audioResource, audio)) {
    delete audio;
    return kInvalidResourceId;
  }
  // Return audio PP_Resource
  return audioResource;
}

PP_Bool IsAudio(PP_Resource resource) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t out_bool;
  NaClSrpcError retval =
      PpbAudioDevRpcClient::PPB_Audio_Dev_IsAudio(
          channel,
          resource,
          &out_bool);
  if (NACL_SRPC_RESULT_OK != retval) {
    return PP_FALSE;
  }
  return pp::BoolToPPBool(out_bool);
}

PP_Resource GetCurrentConfig(PP_Resource audio) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  PP_Resource out_resource;
  NaClSrpcError retval =
      PpbAudioDevRpcClient::PPB_Audio_Dev_GetCurrentConfig(
          channel,
          audio,
          &out_resource);
  if (NACL_SRPC_RESULT_OK != retval) {
    return kInvalidResourceId;
  }
  return out_resource;
}

PP_Bool StartPlayback(PP_Resource audioResource) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t out_bool;
  AudioProxy* audio = GetBoundAudio(audioResource);
  if (NULL == audio) {
    return PP_FALSE;
  }
  if (audio->state() == AUDIO_INCOMPLETE) {
    audio->set_state(AUDIO_PENDING);
  }
  if (audio->state() == AUDIO_READY) {
    if (!audio->StartAudioThread()) {
      return PP_FALSE;
    }
  }
  NaClSrpcError retval =
      PpbAudioDevRpcClient::PPB_Audio_Dev_StartPlayback(
          channel,
          audioResource,
          &out_bool);
  if (NACL_SRPC_RESULT_OK != retval) {
    return PP_FALSE;
  }
  if (PP_FALSE == out_bool) {
    return PP_FALSE;
  }
  return PP_TRUE;
}

PP_Bool StopPlayback(PP_Resource audioResource) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t out_bool;
  AudioProxy* audio = GetBoundAudio(audioResource);
  if (NULL == audio) {
    return PP_FALSE;
  }
  if (audio->state() == AUDIO_PENDING) {
    // audio is pending to start, but StreamCreated() hasn't occurred yet...
    audio->set_state(AUDIO_INCOMPLETE);
  }
  // RPC to trusted side
  NaClSrpcError retval =
      PpbAudioDevRpcClient::PPB_Audio_Dev_StopPlayback(
          channel,
          audioResource,
          &out_bool);
  if (NACL_SRPC_RESULT_OK != retval) {
    return PP_FALSE;
  }
  if (audio->state() != AUDIO_PLAYING) {
    return pp::BoolToPPBool(out_bool);
  }
  // stop and join the audio thread
  return pp::BoolToPPBool(audio->StopAudioThread());
}

}  // namespace

const PPB_Audio_Dev* PluginAudio::GetInterface() {
  static const PPB_Audio_Dev intf = {
    Create,
    IsAudio,
    GetCurrentConfig,
    StartPlayback,
    StopPlayback,
  };
  return &intf;
}

}  // namespace ppapi_proxy

// PppAudioDevRpcServer::PPP_Audio_Dev_StreamCreated() must be in global
// namespace.  This function receives handles for the socket and shared
// memory, provided by the trusted audio implementation.
void PppAudioDevRpcServer::PPP_Audio_Dev_StreamCreated(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource audioResource,
    NaClSrpcImcDescType shm,
    int32_t shm_size,
    NaClSrpcImcDescType sync_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  ppapi_proxy::AudioProxy* audio = ppapi_proxy::GetBoundAudio(audioResource);
  if (NULL == audio) {
    // Ignore if no audioResource -> audioInstance mapping exists,
    // the app may have shutdown audio before StreamCreated() invoked.
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  audio->StreamCreated(sync_socket, shm, shm_size);
  rpc->result = NACL_SRPC_RESULT_OK;
}
