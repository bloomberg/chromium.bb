// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_audio_config.h"

#include <stdio.h>
#include <string.h>
#include "srpcgen/ppb_rpc.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/dev/ppb_audio_config_dev.h"

namespace ppapi_proxy {
namespace {

PP_AudioSampleRate_Dev GetSampleRate(PP_Resource config) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t sample_rate;
  NaClSrpcError retval =
      PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_GetSampleRate(
          channel,
          config,
          &sample_rate);
  if (NACL_SRPC_RESULT_OK == retval) {
    return static_cast<PP_AudioSampleRate_Dev>(sample_rate);
  }
  return PP_AUDIOSAMPLERATE_NONE;
}

uint32_t GetSampleFrameCount(PP_Resource config) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t sample_frame_count;
  NaClSrpcError retval =
      PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_GetSampleFrameCount(
          channel,
          config,
          &sample_frame_count);
  if (NACL_SRPC_RESULT_OK == retval) {
    return static_cast<int32_t>(sample_frame_count);
  }
  return 0;
}

uint32_t RecommendSampleFrameCount(uint32_t request_sample_frame_count) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t out_sample_frame_count;
  NaClSrpcError retval =
      PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_RecommendSampleFrameCount(
          channel,
          request_sample_frame_count,
          &out_sample_frame_count);
  if (NACL_SRPC_RESULT_OK == retval) {
    return static_cast<uint32_t>(out_sample_frame_count);
  }
  return 0;
}

PP_Bool IsAudioConfig(PP_Resource resource) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  int32_t out_bool;
  NaClSrpcError retval =
      PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_IsAudioConfig(
          channel,
          resource,
          &out_bool);
  if (NACL_SRPC_RESULT_OK == retval) {
      return out_bool ? PP_TRUE : PP_FALSE;
  }
  return PP_FALSE;
}

PP_Resource CreateStereo16Bit(PP_Instance instance,
                              PP_AudioSampleRate_Dev sample_rate,
                              uint32_t sample_frame_count) {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  PP_Resource resource;
  NaClSrpcError retval =
      PpbAudioConfigDevRpcClient::PPB_AudioConfig_Dev_CreateStereo16Bit(
          channel,
          instance,
          static_cast<int32_t>(sample_rate),
          static_cast<int32_t>(sample_frame_count),
          &resource);
  if (NACL_SRPC_RESULT_OK == retval) {
    return resource;
  }
  return kInvalidResourceId;
}
}  // namespace

const PPB_AudioConfig_Dev* PluginAudioConfig::GetInterface() {
  static const PPB_AudioConfig_Dev intf = {
    CreateStereo16Bit,
    RecommendSampleFrameCount,
    IsAudioConfig,
    GetSampleRate,
    GetSampleFrameCount,
  };
  return &intf;
}
}  // namespace ppapi_proxy
