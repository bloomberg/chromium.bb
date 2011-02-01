// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_AudioConfig functions.

#include "ppapi/c/ppb_audio_config.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "srpcgen/ppb_rpc.h"

static const PPB_AudioConfig* GetAudioConfigInterface() {
  static const PPB_AudioConfig* audioConfig =
      static_cast<const PPB_AudioConfig*>
          (ppapi_proxy::GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE));
  return audioConfig;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_CreateStereo16Bit(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t sample_rate,
    int32_t sample_frame_count,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  if (NULL == resource) {
    return;
  }
  *resource = audio->CreateStereo16Bit(
      instance, static_cast<PP_AudioSampleRate>(sample_rate),
      sample_frame_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_RecommendSampleFrameCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t sample_rate,
    int32_t request_sample_frame_count,
    int32_t* sample_frame_count) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  *sample_frame_count = audio->RecommendSampleFrameCount(
      static_cast<PP_AudioSampleRate>(sample_rate),
      request_sample_frame_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_IsAudioConfig(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* bool_out) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  *bool_out = static_cast<int32_t>(audio->IsAudioConfig(resource));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_GetSampleRate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* sample_rate) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  if (ppapi_proxy::kInvalidResourceId == resource) {
    return;
  }
  if (NULL == sample_rate) {
    return;
  }
  *sample_rate = audio->GetSampleRate(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_GetSampleFrameCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* sample_frame_count) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  if (ppapi_proxy::kInvalidResourceId == resource) {
    return;
  }
  if (NULL == sample_frame_count) {
    return;
  }
  *sample_frame_count = audio->GetSampleFrameCount(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}
