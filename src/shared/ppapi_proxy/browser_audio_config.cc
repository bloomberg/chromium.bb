// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/dev/ppb_audio_config_dev.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "srpcgen/ppb_rpc.h"

static const PPB_AudioConfig_Dev* GetAudioConfigInterface() {
  static const PPB_AudioConfig_Dev* audioConfig =
      static_cast<const PPB_AudioConfig_Dev*>
          (ppapi_proxy::GetBrowserInterface(PPB_AUDIO_CONFIG_DEV_INTERFACE));
  return audioConfig;
}

void PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_CreateStereo16Bit(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t module,
    int32_t sample_rate,
    int32_t sample_frame_count,
    int64_t* resource) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig_Dev* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  if (NULL == resource) {
    return;
  }
  *resource = audio->CreateStereo16Bit(
      module, static_cast<PP_AudioSampleRate_Dev>(sample_rate),
      sample_frame_count);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_RecommendSampleFrameCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t request,
    int32_t* sample_frame_count) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig_Dev* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  *sample_frame_count = audio->RecommendSampleFrameCount(request);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_IsAudioConfig(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t resource,
    int32_t* bool_out) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig_Dev* audio = GetAudioConfigInterface();
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  if (NULL == audio) {
    return;
  }
  *bool_out = static_cast<int32_t>(audio->IsAudioConfig(resource));
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_GetSampleRate(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t resource,
    int32_t* sample_rate) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig_Dev* audio = GetAudioConfigInterface();
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

void PpbAudioConfigDevRpcServer::PPB_AudioConfig_Dev_GetSampleFrameCount(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int64_t resource,
    int32_t* sample_frame_count) {
  NaClSrpcClosureRunner runner(done);
  const PPB_AudioConfig_Dev* audio = GetAudioConfigInterface();
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
