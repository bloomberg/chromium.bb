// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_audio_config.h"

#include <stdio.h>
#include <string.h>
#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppb_audio_config.h"

namespace ppapi_proxy {

namespace {

PP_AudioSampleRate GetSampleRate(PP_Resource config) {
  DebugPrintf("PPB_AudioConfig::GetSampleRate: config=%"NACL_PRIu32"\n",
              config);
  int32_t sample_rate;
  NaClSrpcError srpc_result =
      PpbAudioConfigRpcClient::PPB_AudioConfig_GetSampleRate(
          GetMainSrpcChannel(),
          config,
          &sample_rate);
  DebugPrintf("PPB_AudioConfig::GetSampleRate: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK == srpc_result) {
    return static_cast<PP_AudioSampleRate>(sample_rate);
  }
  return PP_AUDIOSAMPLERATE_NONE;
}

uint32_t GetSampleFrameCount(PP_Resource config) {
  DebugPrintf("PPB_AudioConfig::GetSampleFrameCount: config=%"NACL_PRIu32"\n",
              config);
  int32_t sample_frame_count;
  NaClSrpcError srpc_result =
      PpbAudioConfigRpcClient::PPB_AudioConfig_GetSampleFrameCount(
          GetMainSrpcChannel(),
          config,
          &sample_frame_count);
  DebugPrintf("PPB_AudioConfig::GetSampleFrameCount: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK == srpc_result) {
    return static_cast<uint32_t>(sample_frame_count);
  }
  return 0;
}

uint32_t RecommendSampleFrameCount(PP_AudioSampleRate sample_rate,
                                   uint32_t request_sample_frame_count) {
  DebugPrintf("PPB_AudioConfig::RecommendSampleFrameCount");
  int32_t out_sample_frame_count;
  NaClSrpcError srpc_result =
      PpbAudioConfigRpcClient::PPB_AudioConfig_RecommendSampleFrameCount(
          GetMainSrpcChannel(),
          static_cast<int32_t>(sample_rate),
          static_cast<int32_t>(request_sample_frame_count),
          &out_sample_frame_count);
  DebugPrintf("PPB_AudioConfig::RecommendSampleFrameCount: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK == srpc_result) {
    return static_cast<uint32_t>(out_sample_frame_count);
  }
  return 0;
}

PP_Bool IsAudioConfig(PP_Resource resource) {
  DebugPrintf("PPB_AudioConfig::IsAudioConfig: resource=%"NACL_PRIu32"\n",
              resource);
  int32_t success;
  NaClSrpcError srpc_result =
      PpbAudioConfigRpcClient::PPB_AudioConfig_IsAudioConfig(
          GetMainSrpcChannel(),
          resource,
          &success);
  DebugPrintf("PPB_AudioConfig::IsAudioConfig: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK == srpc_result && success) {
    return PP_TRUE;
  }
  return PP_FALSE;
}

PP_Resource CreateStereo16Bit(PP_Instance instance,
                              PP_AudioSampleRate sample_rate,
                              uint32_t sample_frame_count) {
  DebugPrintf("PPB_AudioConfig::CreateStereo16Bit: instance=%"NACL_PRIu32"\n",
              instance);
  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbAudioConfigRpcClient::PPB_AudioConfig_CreateStereo16Bit(
          GetMainSrpcChannel(),
          instance,
          static_cast<int32_t>(sample_rate),
          static_cast<int32_t>(sample_frame_count),
          &resource);
  DebugPrintf("PPB_AudioConfig::CreateStereo16Bit: %s\n",
              NaClSrpcErrorString(srpc_result));
  if (NACL_SRPC_RESULT_OK == srpc_result) {
    return resource;
  }
  return kInvalidResourceId;
}
}  // namespace

const PPB_AudioConfig* PluginAudioConfig::GetInterface() {
  static const PPB_AudioConfig audio_config_interface = {
    CreateStereo16Bit,
    RecommendSampleFrameCount,
    IsAudioConfig,
    GetSampleRate,
    GetSampleFrameCount,
  };
  return &audio_config_interface;
}
}  // namespace ppapi_proxy
