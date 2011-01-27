// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_audio.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/common.h"
#include "ppapi/cpp/module_impl.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {
namespace {

// syscall_read() implemented here as a NACL_SYSCALL to avoid
// conflicts with projects that attempt to wrap the read() call
// using linker command line option "--wrap read"
ssize_t syscall_read(int desc, void *buf, size_t count) {
  ssize_t retval = NACL_SYSCALL(read)(desc, buf, count);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}

// syscall_close() implemented here as a NACL_SYSCALL to avoid
// conflicts with projects that attempt to wrap the close() call
// using linker command line option "--wrap close"
ssize_t syscall_close(int desc) {
  ssize_t retval = NACL_SYSCALL(close)(desc);
  if (retval < 0) {
    errno = -retval;
    return -1;
  }
  return retval;
}

// round size up to next 64k
size_t ceil64k(size_t n) {
  return (n + 0xFFFF) & (~0xFFFF);
}

}  // namespace

PluginAudio::PluginAudio()
    : socket_(-1),
    shm_(-1),
    shm_size_(0),
    shm_buffer_(NULL),
    state_(AUDIO_INCOMPLETE),
    thread_id_(),
    thread_active_(false),
    user_callback_(NULL),
    user_data_(NULL) {
  DebugPrintf("PluginAudio::PluginAudio()\n");
}

PluginAudio::~PluginAudio() {
  DebugPrintf("PluginAudio::~PluginAudio()\n");
  // Stop the audio thread, if one is currently running.
  if (thread_active_) {
    StopAudioThread();
  }
  // Unmap the shared memory buffer, if present.
  if (shm_buffer_) {
    munmap(shm_buffer_, ceil64k(shm_size_));
    shm_buffer_ = NULL;
    shm_size_ = 0;
  }
  // Close the handles.
  if (shm_ != -1) {
    syscall_close(shm_);
    shm_ = -1;
  }
  if (socket_ != -1) {
    syscall_close(socket_);
    socket_ = -1;
  }
}

bool PluginAudio::InitFromBrowserResource(PP_Resource resource) {
  DebugPrintf("PluginAudio::InitFromBrowserResource resource=%"NACL_PRIx32"\n",
              resource);
  return true;
}

void* PluginAudio::AudioThread(void* self) {
  PluginAudio* audio = static_cast<PluginAudio*>(self);
  DebugPrintf("PluginAudio::AudioThread self=%p\n", self);
  while (true) {
    int32_t sync_value;
    // block on socket read
    ssize_t r = syscall_read(audio->socket_,
                             &sync_value, sizeof(sync_value));
    // StopPlayback() will send a value of -1 over the sync_socket
    if ((sizeof(sync_value) != r) || (-1 == sync_value))
      break;
    // invoke user callback, get next buffer of audio data
    audio->user_callback_(audio->shm_buffer_,
                          audio->shm_size_,
                          audio->user_data_);
  }
  return NULL;
}

void PluginAudio::StreamCreated(NaClSrpcImcDescType socket,
      NaClSrpcImcDescType shm, size_t shm_size) {
  DebugPrintf("PluginAudio::StreamCreated shm=%"NACL_PRIx32""
              " shm_size=%"NACL_PRIuS"\n", shm, shm_size);
  socket_ = socket;
  shm_ = shm;
  shm_size_ = shm_size;
  shm_buffer_ = mmap(NULL,
                     ceil64k(shm_size),
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

bool PluginAudio::StartAudioThread() {
  // clear contents of shm buffer before spinning up audio thread
  DebugPrintf("PluginAudio::StartAudioThread\n");
  memset(shm_buffer_, 0, shm_size_);
  int ret = pthread_create(&thread_id_, NULL, AudioThread, this);
  if (0 == ret) {
    thread_active_ = true;
    set_state(AUDIO_PLAYING);
    return true;
  }
  return false;
}

bool PluginAudio::StopAudioThread() {
  DebugPrintf("PluginAudio::StopAudioThread\n");
  if (thread_active_) {
    int ret = pthread_join(thread_id_, NULL);
    if (0 == ret) {
      thread_active_ = false;
      set_state(AUDIO_READY);
      return true;
    }
  }
  return false;
}

// Start of untrusted PPB_Audio functions
namespace {

PP_Resource Create(PP_Instance instance,
                   PP_Resource config,
                   PPB_Audio_Callback user_callback,
                   void* user_data) {
  DebugPrintf("PPB_Audio -- Create instance=%"NACL_PRIx32""
              " config=%"NACL_PRIx32" user_callback=%"NACL_PRIx32""
              " user_data=%p\n",
               instance, config, user_callback, user_data);
  PP_Resource audioResource;
  NaClSrpcError retval = NACL_SRPC_RESULT_OK;
  NaClSrpcChannel* channel = NULL;
  // Proxy to browser Create, get audio PP_Resource
  channel = ppapi_proxy::GetMainSrpcChannel();
  retval = PpbAudioRpcClient::PPB_Audio_Create(
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
  scoped_refptr<PluginAudio> audio =
      PluginResource::AdoptAs<PluginAudio>(audioResource);
  if (audio.get()) {
    audio->set_callback(user_callback, user_data);
    return audioResource;
  }
  return kInvalidResourceId;
}

PP_Bool IsAudio(PP_Resource resource) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t out_bool;
  DebugPrintf("IsAudio resource=%"NACL_PRIx32"\n", resource);
  NaClSrpcError retval =
      PpbAudioRpcClient::PPB_Audio_IsAudio(
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
  DebugPrintf("GetCurrentConfig audio=%"NACL_PRIx32"\n", audio);
  NaClSrpcError retval =
      PpbAudioRpcClient::PPB_Audio_GetCurrentConfig(
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
  scoped_refptr<PluginAudio> audio =
      PluginResource::GetAs<PluginAudio>(audioResource);
  DebugPrintf("StartPlayback audioResource=%"NACL_PRIx32"\n", audioResource);

  if (NULL == audio.get()) {
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
      PpbAudioRpcClient::PPB_Audio_StartPlayback(
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
  DebugPrintf("StopPlayback audioResource=%"NACL_PRIx32"\n", audioResource);
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t out_bool;
  scoped_refptr<PluginAudio> audio =
      PluginResource::GetAs<PluginAudio>(audioResource);

  if (NULL == audio.get()) {
    return PP_FALSE;
  }
  if (audio->state() == AUDIO_PENDING) {
    // audio is pending to start, but StreamCreated() hasn't occurred yet...
    audio->set_state(AUDIO_INCOMPLETE);
  }
  // RPC to trusted side
  NaClSrpcError retval =
      PpbAudioRpcClient::PPB_Audio_StopPlayback(
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

const PPB_Audio* PluginAudio::GetInterface() {
  DebugPrintf("PluginAudio::GetInterface\n");
  static const PPB_Audio intf = {
    Create,
    IsAudio,
    GetCurrentConfig,
    StartPlayback,
    StopPlayback,
  };
  return &intf;
}
}  // namespace ppapi_proxy

using ppapi_proxy::DebugPrintf;

// PppAudioRpcServer::PPP_Audio_StreamCreated() must be in global
// namespace.  This function receives handles for the socket and shared
// memory, provided by the trusted audio implementation.
void PppAudioRpcServer::PPP_Audio_StreamCreated(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource audioResource,
    NaClSrpcImcDescType shm,
    int32_t shm_size,
    NaClSrpcImcDescType sync_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PppAudioRpcServer::PPP_Audio_StreamCreated"
              " rpc=%"NACL_PRIx32" done=%"NACL_PRIx32""
              " audioResource=%"NACL_PRIx32" shm=%"NACL_PRIx32""
              " shm_size=%"NACL_PRIuS" sync_socket=%"NACL_PRIx32"\n",
              rpc, done, audioResource, shm, shm_size, sync_socket);
  scoped_refptr<ppapi_proxy::PluginAudio> audio =
      ppapi_proxy::PluginResource::
      GetAs<ppapi_proxy::PluginAudio>(audioResource);
  if (NULL == audio.get()) {
    // Ignore if no audioResource -> audioInstance mapping exists,
    // the app may have shutdown audio before StreamCreated() invoked.
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  audio->StreamCreated(sync_socket, shm, shm_size);
  rpc->result = NACL_SRPC_RESULT_OK;
}
