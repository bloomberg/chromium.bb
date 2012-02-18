// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_AudioConfig functions.

#include "ppapi/c/ppb_audio_config.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;

static const PPB_AudioConfig_1_0* GetAudioConfigInterface_1_0() {
  static const PPB_AudioConfig_1_0* audio_config =
      static_cast<const PPB_AudioConfig_1_0*>
          (ppapi_proxy::GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE_1_0));
  return audio_config;
}

static const PPB_AudioConfig* GetAudioConfigInterface() {
  static const PPB_AudioConfig* audio_config =
      static_cast<const PPB_AudioConfig*>
          (ppapi_proxy::GetBrowserInterface(PPB_AUDIO_CONFIG_INTERFACE));
  return audio_config;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_CreateStereo16Bit(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t sample_rate,
    int32_t sample_frame_count,
    PP_Resource* resource) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio_config = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio_config) {
    return;
  }
  if (NULL == resource) {
    return;
  }
  *resource = audio_config->CreateStereo16Bit(
      instance, static_cast<PP_AudioSampleRate>(sample_rate),
      sample_frame_count);
  DebugPrintf("PPB_AudioConfig::CreateStereo16Bit: resource=%"NACL_PRId32"\n",
      *resource);
  DebugPrintf(
      "PPB_AudioConfig::CreateStereo16Bit: sample_rate=%"NACL_PRIu32"\n",
      sample_rate);
  DebugPrintf(
      "PPB_AudioConfig::CreateStereo16Bit: frame_count=%"NACL_PRIu32"\n",
      sample_frame_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Preserve old behavior for applications that request 1.0 interface.
void PpbAudioConfigRpcServer::PPB_AudioConfig_RecommendSampleFrameCount_1_0(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t sample_rate,
    int32_t request_sample_frame_count,
    int32_t* sample_frame_count) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig_1_0* audio_config = GetAudioConfigInterface_1_0();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio_config) {
    return;
  }
  *sample_frame_count = audio_config->RecommendSampleFrameCount(
      static_cast<PP_AudioSampleRate>(sample_rate),
      request_sample_frame_count);
  DebugPrintf("PPB_AudioConfig::RecommendSampleFrameCount_1_0: "
              "sample_frame_count=%"NACL_PRId32"\n", *sample_frame_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_RecommendSampleFrameCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t sample_rate,
    int32_t request_sample_frame_count,
    int32_t* sample_frame_count) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio_config = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio_config) {
    return;
  }
  *sample_frame_count = audio_config->RecommendSampleFrameCount(
      instance,
      static_cast<PP_AudioSampleRate>(sample_rate),
      request_sample_frame_count);
  DebugPrintf("PPB_AudioConfig::RecommendSampleFrameCount: "
              "sample_frame_count=%"NACL_PRId32"\n", *sample_frame_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_IsAudioConfig(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio_config = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio_config) {
    return;
  }
  PP_Bool pp_success = audio_config->IsAudioConfig(resource);
  *success = PP_ToBool(pp_success);
  DebugPrintf("PPB_AudioConfig::IsAudioConfig: success=%d\n", *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_GetSampleRate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* sample_rate) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio_config = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio_config) {
    return;
  }
  if (ppapi_proxy::kInvalidResourceId == resource) {
    return;
  }
  if (NULL == sample_rate) {
    return;
  }
  *sample_rate = audio_config->GetSampleRate(resource);
  DebugPrintf("PPB_AudioConfig::GetSampleRate: pp_success=%"NACL_PRId32"\n",
              *sample_rate);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_GetSampleFrameCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* sample_frame_count) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio_config = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio_config) {
    return;
  }
  if (ppapi_proxy::kInvalidResourceId == resource) {
    return;
  }
  if (NULL == sample_frame_count) {
    return;
  }
  *sample_frame_count = audio_config->GetSampleFrameCount(resource);
  DebugPrintf("PPB_AudioConfig::GetSampleFrameCount: "
              "sample_frame_count=%"NACL_PRId32"\n", *sample_frame_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigRpcServer::PPB_AudioConfig_RecommendSampleRate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t* sample_rate) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig* audio_config = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio_config) {
    return;
  }
  *sample_rate = audio_config->RecommendSampleRate(instance);
  DebugPrintf("PPB_AudioConfig::RecommendSampleRate: "
              "sample_rate=%"NACL_PRIu32"\n", *sample_rate);
  rpc->result = NACL_SRPC_RESULT_OK;
}
