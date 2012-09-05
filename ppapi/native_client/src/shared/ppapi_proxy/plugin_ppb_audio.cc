// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_audio.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/mman.h>
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_audio_config.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "media/audio/shared_memory_util.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/cpp/module_impl.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {
namespace {

// Round size up to next 64k; required for NaCl's version of mmap().
size_t ceil64k(size_t n) {
  return (n + 0xFFFF) & (~0xFFFF);
}

// Hard coded values from PepperPlatformAudioOutputImpl.
// TODO(dalecurtis): PPAPI shouldn't hard code these values for all clients.
enum { kChannels = 2, kBytesPerSample = 2 };

}  // namespace

PluginAudio::PluginAudio() :
    resource_(kInvalidResourceId),
    socket_(-1),
    shm_(-1),
    shm_size_(0),
    shm_buffer_(NULL),
    state_(AUDIO_INCOMPLETE),
    thread_id_(),
    thread_active_(false),
    user_callback_(NULL),
    user_data_(NULL),
    client_buffer_size_bytes_(0) {
  DebugPrintf("PluginAudio::PluginAudio\n");
}

PluginAudio::~PluginAudio() {
  DebugPrintf("PluginAudio::~PluginAudio\n");
  // Ensure audio thread is not active.
  if (resource_ != kInvalidResourceId)
    GetInterface()->StopPlayback(resource_);
  // Unmap the shared memory buffer, if present.
  if (shm_buffer_) {
    audio_bus_.reset();
    client_buffer_.reset();
    munmap(shm_buffer_,
           ceil64k(media::TotalSharedMemorySizeInBytes(shm_size_)));
    shm_buffer_ = NULL;
    shm_size_ = 0;
    client_buffer_size_bytes_ = 0;
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
}

bool PluginAudio::InitFromBrowserResource(PP_Resource resource) {
  DebugPrintf("PluginAudio::InitFromBrowserResource: resource=%"NACL_PRId32"\n",
              resource);
  resource_ = resource;
  return true;
}

void PluginAudio::AudioThread(void* self) {
  PluginAudio* audio = static_cast<PluginAudio*>(self);
  DebugPrintf("PluginAudio::AudioThread: self=%p\n", self);
  const int bytes_per_frame =
      sizeof(*(audio->audio_bus_->channel(0))) * audio->audio_bus_->channels();
  while (true) {
    int32_t sync_value;
    // Block on socket read.
    ssize_t r = read(audio->socket_, &sync_value, sizeof(sync_value));
    // StopPlayback() will send a value of -1 over the sync_socket.
    if ((sizeof(sync_value) != r) || (-1 == sync_value))
      break;
    // Invoke user callback, get next buffer of audio data.
    audio->user_callback_(audio->client_buffer_.get(),
                          audio->client_buffer_size_bytes_,
                          audio->user_data_);

    // Deinterleave the audio data into the shared memory as float.
    audio->audio_bus_->FromInterleaved(
        audio->client_buffer_.get(), audio->audio_bus_->frames(),
        kBytesPerSample);

    // Signal audio backend by writing buffer length at end of buffer.
    // (Note: NaCl applications will always write the entire buffer.)
    // TODO(dalecurtis): Technically this is not the exact size.  Due to channel
    // padding for alignment, there may be more data available than this.  We're
    // relying on AudioSyncReader::Read() to parse this with that in mind.
    // Rename these methods to Set/GetActualFrameCount().
    media::SetActualDataSizeInBytes(
        audio->shm_buffer_, audio->shm_size_,
        audio->audio_bus_->frames() * bytes_per_frame);
  }
}

void PluginAudio::StreamCreated(NaClSrpcImcDescType socket,
      NaClSrpcImcDescType shm, size_t shm_size) {
  DebugPrintf("PluginAudio::StreamCreated: shm=%"NACL_PRIu32""
              " shm_size=%"NACL_PRIuS"\n", shm, shm_size);
  socket_ = socket;
  shm_ = shm;
  shm_size_ = shm_size;
  shm_buffer_ = mmap(NULL,
                     ceil64k(media::TotalSharedMemorySizeInBytes(shm_size)),
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     shm,
                     0);
  if (MAP_FAILED != shm_buffer_) {
    PP_Resource ac = GetInterface()->GetCurrentConfig(resource_);
    int frames = PluginAudioConfig::GetInterface()->GetSampleFrameCount(ac);

    audio_bus_ = media::AudioBus::WrapMemory(kChannels, frames, shm_buffer_);
    // Setup integer audio buffer for user audio data.
    client_buffer_size_bytes_ =
        audio_bus_->frames() * audio_bus_->channels() * kBytesPerSample;
    client_buffer_.reset(new uint8_t[client_buffer_size_bytes_]);

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
  memset(client_buffer_.get(), 0, client_buffer_size_bytes_);
  const struct PP_ThreadFunctions* thread_funcs = GetThreadCreator();
  if (NULL == thread_funcs->thread_create ||
      NULL == thread_funcs->thread_join) {
    return false;
  }
  int ret = thread_funcs->thread_create(&thread_id_, AudioThread, this);
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
    int ret = GetThreadCreator()->thread_join(thread_id_);
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
  DebugPrintf("PPB_Audio::Create: instance=%"NACL_PRId32" config=%"NACL_PRId32
              " user_callback=%p user_data=%p\n",
              instance, config, user_callback, user_data);
  if (NULL == user_callback) {
    return kInvalidResourceId;
  }
  PP_Resource audio_resource;
  // Proxy to browser Create, get audio PP_Resource
  NaClSrpcError srpc_result = PpbAudioRpcClient::PPB_Audio_Create(
      GetMainSrpcChannel(),
      instance,
      config,
      &audio_resource);
  DebugPrintf("PPB_Audio::Create: %s\n", NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return kInvalidResourceId;
  }
  if (kInvalidResourceId == audio_resource) {
    return kInvalidResourceId;
  }
  scoped_refptr<PluginAudio> audio =
      PluginResource::AdoptAs<PluginAudio>(audio_resource);
  if (audio.get()) {
    audio->set_callback(user_callback, user_data);
    return audio_resource;
  }
  return kInvalidResourceId;
}

PP_Bool IsAudio(PP_Resource resource) {
  int32_t success;
  DebugPrintf("PPB_Audio::IsAudio: resource=%"NACL_PRId32"\n", resource);
  NaClSrpcError srpc_result =
      PpbAudioRpcClient::PPB_Audio_IsAudio(
          GetMainSrpcChannel(),
          resource,
          &success);
  DebugPrintf("PPB_Audio::IsAudio: %s\n", NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return PP_FALSE;
  }
  return PP_FromBool(success);
}

PP_Resource GetCurrentConfig(PP_Resource audio) {
  DebugPrintf("PPB_Audio::GetCurrentConfig: audio=%"NACL_PRId32"\n", audio);
  PP_Resource config_resource;
  NaClSrpcError srpc_result =
      PpbAudioRpcClient::PPB_Audio_GetCurrentConfig(
          GetMainSrpcChannel(),
          audio,
          &config_resource);
  DebugPrintf("PPB_Audio::GetCurrentConfig: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return kInvalidResourceId;
  }
  return config_resource;
}

PP_Bool StartPlayback(PP_Resource audio_resource) {
  DebugPrintf("PPB_Audio::StartPlayback: audio_resource=%"NACL_PRId32"\n",
              audio_resource);
  scoped_refptr<PluginAudio> audio =
      PluginResource::GetAs<PluginAudio>(audio_resource);
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
  int32_t success;
  NaClSrpcError srpc_result =
      PpbAudioRpcClient::PPB_Audio_StartPlayback(
          GetMainSrpcChannel(),
          audio_resource,
          &success);
  DebugPrintf("PPB_Audio::StartPlayback: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK != srpc_result || !success) {
    return PP_FALSE;
  }
  return PP_TRUE;
}

PP_Bool StopPlayback(PP_Resource audio_resource) {
  DebugPrintf("PPB_Audio::StopPlayback: audio_resource=%"NACL_PRId32"\n",
              audio_resource);
  scoped_refptr<PluginAudio> audio =
      PluginResource::GetAs<PluginAudio>(audio_resource);
  if (NULL == audio.get()) {
    return PP_FALSE;
  }
  if (audio->state() == AUDIO_PENDING) {
    // audio is pending to start, but StreamCreated() hasn't occurred yet...
    audio->set_state(AUDIO_INCOMPLETE);
  }
  // RPC to trusted side
  int32_t success;
  NaClSrpcError srpc_result =
      PpbAudioRpcClient::PPB_Audio_StopPlayback(
          GetMainSrpcChannel(),
          audio_resource,
          &success);
  DebugPrintf("PPB_Audio::StopPlayback: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK != srpc_result) {
    return PP_FALSE;
  }
  if (audio->state() != AUDIO_PLAYING) {
    return PP_FromBool(success);
  }
  // stop and join the audio thread
  return PP_FromBool(audio->StopAudioThread());
}
}  // namespace

const PPB_Audio* PluginAudio::GetInterface() {
  DebugPrintf("PluginAudio::GetInterface\n");
  static const PPB_Audio audio_interface = {
    Create,
    IsAudio,
    GetCurrentConfig,
    StartPlayback,
    StopPlayback,
  };
  return &audio_interface;
}
}  // namespace ppapi_proxy

using ppapi_proxy::DebugPrintf;

// PppAudioRpcServer::PPP_Audio_StreamCreated() must be in global
// namespace.  This function receives handles for the socket and shared
// memory, provided by the trusted audio implementation.
void PppAudioRpcServer::PPP_Audio_StreamCreated(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource audio_resource,
    NaClSrpcImcDescType shm,
    int32_t shm_size,
    NaClSrpcImcDescType sync_socket) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPP_Audio::StreamCreated: audio_resource=%"NACL_PRId32
              " shm=%"NACL_PRIx32" shm_size=%"NACL_PRIuS
              " sync_socket=%"NACL_PRIx32"\n",
              audio_resource, shm, shm_size, sync_socket);
  scoped_refptr<ppapi_proxy::PluginAudio> audio =
      ppapi_proxy::PluginResource::
      GetAs<ppapi_proxy::PluginAudio>(audio_resource);
  if (NULL == audio.get()) {
    // Ignore if no audio_resource -> audio_instance mapping exists,
    // the app may have shutdown audio before StreamCreated() invoked.
    rpc->result = NACL_SRPC_RESULT_OK;
    return;
  }
  audio->StreamCreated(sync_socket, shm, shm_size);
  rpc->result = NACL_SRPC_RESULT_OK;
}
