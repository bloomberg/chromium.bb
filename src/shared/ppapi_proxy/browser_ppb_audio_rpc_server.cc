// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Audio functions.

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_invalid.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/trusted/ppb_audio_trusted.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/module.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;

namespace {

const PPB_AudioTrusted* GetAudioTrustedInterface() {
  DebugPrintf("GetAudioTrustedInterface\n");
  static const PPB_AudioTrusted* audioTrusted =
      static_cast<const PPB_AudioTrusted*>
          (ppapi_proxy::GetBrowserInterface(PPB_AUDIO_TRUSTED_INTERFACE));
  return audioTrusted;
}

const PPB_Audio* GetAudioInterface() {
  DebugPrintf("GetAudioInterface\n");
  static const PPB_Audio* audio =
      static_cast<const PPB_Audio*>
          (ppapi_proxy::GetBrowserInterface(PPB_AUDIO_INTERFACE));
  return audio;
}

struct StreamCreatedCallbackData {
  PP_Instance instance_id;
  PP_Resource audio_id;
  StreamCreatedCallbackData(PP_Instance i, PP_Resource a) :
      instance_id(i),
      audio_id(a) { }
};

// This completion callback will be invoked when the sync socket and shared
// memory handles become available.
void StreamCreatedCallback(void* user_data, int32_t result) {
  DebugPrintf("StreamCreatedCallback: user_data=%p result=%"NACL_PRId32"\n",
              user_data, result);
  if (NULL == user_data) {
    return;
  }
  nacl::scoped_ptr<StreamCreatedCallbackData> data(
      static_cast<StreamCreatedCallbackData*>(user_data));
  if (result < 0) {
    return;
  }
  const PPB_AudioTrusted* audioTrusted = GetAudioTrustedInterface();
  if (NULL == audioTrusted) {
    return;
  }
  int sync_socket_handle;
  int shared_memory_handle;
  uint32_t shared_memory_size;
  if (PP_OK != audioTrusted->GetSyncSocket(data->audio_id,
                                           &sync_socket_handle)) {
    return;
  }
  if (PP_OK != audioTrusted->GetSharedMemory(data->audio_id,
                                             &shared_memory_handle,
                                             &shared_memory_size)) {
    return;
  }
  nacl::DescWrapperFactory factory;
  NaClHandle nacl_shm_handle = (NaClHandle)shared_memory_handle;
  NaClHandle nacl_sync_handle = (NaClHandle)sync_socket_handle;
  nacl::scoped_ptr<nacl::DescWrapper> shm_wrapper(factory.ImportShmHandle(
      nacl_shm_handle, shared_memory_size));
  nacl::scoped_ptr<nacl::DescWrapper> socket_wrapper(
      factory.ImportSyncSocketHandle(nacl_sync_handle));
  NaClDesc *nacl_shm = NaClDescRef(shm_wrapper->desc());
  NaClDesc *nacl_socket = NaClDescRef(socket_wrapper->desc());
  int r;
  r = PppAudioRpcClient::PPP_Audio_StreamCreated(
      ppapi_proxy::GetMainSrpcChannel(data->instance_id),
      data->audio_id,
      nacl_shm,
      shared_memory_size,
      nacl_socket);
}

}  // namespace

void PpbAudioRpcServer::PPB_Audio_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    PP_Resource config,
    PP_Resource* resource) {
  DebugPrintf("PPB_Audio::Create: instance=%"NACL_PRIu32" config=%"NACL_PRIu32
              "\n", instance, config);
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioTrusted* audio = GetAudioTrustedInterface();
  PP_CompletionCallback callback;
  StreamCreatedCallbackData* data;
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (ppapi_proxy::kInvalidResourceId == config) {
    return;
  }
  if (NULL == audio) {
    return;
  }
  *resource = audio->CreateTrusted(instance);
  DebugPrintf("PPB_Audio::Create: resource=%"NACL_PRIu32"\n", *resource);
  PP_Resource audio_id = *resource;
  if (ppapi_proxy::kInvalidResourceId == audio_id) {
    return;
  }
  data = new StreamCreatedCallbackData(instance, audio_id);
  callback = PP_MakeOptionalCompletionCallback(StreamCreatedCallback, data);
  int32_t pp_error = audio->Open(audio_id, config, callback);
  DebugPrintf("PPB_Audio::Create: pp_error=%"NACL_PRIu32"\n", pp_error);
  // If the Open() call failed, pass failure code and explicitly
  // invoke the completion callback, giving it a chance to release data.
  if (pp_error != PP_OK_COMPLETIONPENDING) {
    PP_RunCompletionCallback(&callback, pp_error);
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioRpcServer::PPB_Audio_StartPlayback(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* success) {
  DebugPrintf("PPB_Audio::StartPlayback: resource=%"NACL_PRIu32"\n", resource);
  NaClSrpcClosureRunner runner(done);
  const PPB_Audio* audio = GetAudioInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    *success = false;
    return;
  }
  PP_Bool pp_success = audio->StartPlayback(resource);
  DebugPrintf("PPB_Audio::StartPlayback: pp_success=%d\n", pp_success);
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioRpcServer::PPB_Audio_StopPlayback(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* success) {
  DebugPrintf("PPB_Audio::StopPlayback: resource=%"NACL_PRIu32"\n", resource);
  NaClSrpcClosureRunner runner(done);
  const PPB_Audio* audio = GetAudioInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    *success = false;
    return;
  }
  PP_Bool pp_success = audio->StopPlayback(resource);
  DebugPrintf("PPB_Audio::StopPlayback: pp_success=%d\n", pp_success);
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioRpcServer::PPB_Audio_IsAudio(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* success) {
  DebugPrintf("PPB_Audio::IsAudio: resource=%"NACL_PRIu32"\n", resource);
  NaClSrpcClosureRunner runner(done);
  const PPB_Audio* audio = GetAudioInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    *success = false;
    return;
  }
  PP_Bool pp_success = audio->IsAudio(resource);
  DebugPrintf("PPB_Audio::IsAudio: pp_success=%d\n", pp_success);
  *success = (pp_success == PP_TRUE);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioRpcServer::PPB_Audio_GetCurrentConfig(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    PP_Resource* config) {
  DebugPrintf("PPB_Audio::GetCurrentConfig: resource=%"NACL_PRIu32"\n",
              resource);
  NaClSrpcClosureRunner runner(done);
  const PPB_Audio* audio = GetAudioInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  if (ppapi_proxy::kInvalidResourceId == resource) {
    return;
  }
  *config = audio->GetCurrentConfig(resource);
  DebugPrintf("PPB_Audio::GetCurrentConfig: config=%"NACL_PRIu32"\n", *config);
  rpc->result = NACL_SRPC_RESULT_OK;
}
