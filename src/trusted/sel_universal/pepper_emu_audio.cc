/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file provides limited support for pepper audio emulation
// NOTE: Though, it looks like that multiple audio streams are
// supported, this is not the case at this point.


#include <string.h>
#include <string>

#if (NACL_LINUX)
// for shmem
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
// for sync_sockets
#include <sys/socket.h>
#endif

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/ppb_audio.h"
#include "ppapi/c/ppb_audio_config.h"

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"
#include "native_client/src/trusted/sel_universal/pepper_emu_helper.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"

using nacl::DescWrapperFactory;
using nacl::DescWrapper;

// ======================================================================
namespace {

const int kBytesPerSample = 4;  // 16-bit stereo

const int kRecommendSampleFrameCount = 4 * 1024;
const int kMaxAudioBufferSize = 64 * 1024;

IMultimedia* GlobalMultiMediaInterface = 0;

// ======================================================================
// Note: Just a bunch of fairly unrelated global variables,
// we expect them to be zero initialized.
struct DataAudio {
  int handle_config;

  nacl::DescWrapper* desc_shmem;
  nacl::DescWrapper* desc_sync_in;
  nacl::DescWrapper* desc_sync_out;
  void* addr_audio;
};


struct DataAudioConfig {
  int sample_frequency;
  int sample_frame_count;
};

// For now we handle only one resource each which is sufficient for SDL
Resource<DataAudio> GlobalAudioDataResources(1, "audio");
Resource<DataAudioConfig> GlobalAudioConfigDataResources(1, "audio_config");

// ======================================================================
void AudioCallBack(void* user_data, unsigned char* buffer, int length) {
  NaClLog(2, "AudioCallBack(%p, %p, %d)\n", user_data, buffer, length);
  DataAudio* data = reinterpret_cast<DataAudio*>(user_data);
  // NOTE: we copy the previously filled buffer.
  //       This introduces extra latency but simplifies the design
  //       as we do not have to wait for the nexe to generate the data.
  memcpy(buffer, data->addr_audio, length);
  int value = 0;
  data->desc_sync_in->Write(&value, sizeof value);
}

// From the PPB_Audio API
// PP_Resource Create(PP_Instance instance, PP_Resource config,
//                    PPB_Audio_Callback audio_callback, void* user_data);
// PPB_Audio_Create:ii:i
void PPB_Audio_Create(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  int config_handle = ins[1]->u.ival;
  NaClLog(1, "PPB_Audio_Create(%d, %d)\n", instance, config_handle);

  const int handle = GlobalAudioDataResources.Alloc();
  // for now only support one audio resource
  DataAudio* data = GlobalAudioDataResources.GetDataForHandle(handle);
  data->handle_config = config_handle;
  DataAudioConfig* config_data =
    GlobalAudioConfigDataResources.GetDataForHandle(config_handle);

  nacl::DescWrapperFactory factory;
#if (NACL_WINDOWS || NACL_OSX)
  NaClLog(LOG_ERROR, "HandlerSyncSocketCreate NYI for windows/mac\n");
#else
  nacl::Handle handles[2] = {nacl::kInvalidHandle, nacl::kInvalidHandle};
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, handles) != 0) {
    NaClLog(LOG_FATAL, "cannot create syn sockets\n");
  }
  data->desc_sync_in = factory.ImportSyncSocketHandle(handles[0]);
  data->desc_sync_out = factory.ImportSyncSocketHandle(handles[1]);
#endif
  data->desc_shmem = factory.MakeShm(kMaxAudioBufferSize);
  size_t dummy_size;

  if (data->desc_shmem->Map(&data->addr_audio, &dummy_size)) {
    NaClLog(LOG_FATAL, "cannot map audio shmem\n");
  }
  NaClLog(LOG_INFO, "PPB_Audio_Create: buffer is %p\n", data->addr_audio);

  GlobalMultiMediaInterface->AudioInit16Bit(config_data->sample_frequency,
                                            2,
                                            config_data->sample_frame_count,
                                            &AudioCallBack,
                                            data);
  outs[0]->u.ival = handle;
  NaClLog(1, "PPB_Audio_Create -> %d\n", handle);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);

  UserEvent* event =
    MakeUserEvent(EVENT_TYPE_INIT_AUDIO, 0, handle, data, 0);

  GlobalMultiMediaInterface->PushUserEvent(event);
}

// From the PPB_Audio API
// PP_Bool IsAudio(PP_Resource resource)
// PPB_Audio_IsAudio:i:i
void PPB_Audio_IsAudio(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_IsAudio(%d)\n", handle);
  DataAudio* data = GlobalAudioDataResources.GetDataForHandle(handle);
  // for now be strict to catch problems early
  CHECK(data != NULL);
  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_Audio API
// PP_Resource GetCurrentConfig(PP_Resource audio);
// PPB_Audio_GetCurrentConfig:i:i
void PPB_Audio_GetCurrentConfig(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_GetCurrentConfig(%d)\n", handle);
  DataAudio* data = GlobalAudioDataResources.GetDataForHandle(handle);
  outs[0]->u.ival = data->handle_config;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_Audio API
// PP_Bool StartPlayback(PP_Resource audio);
// PPB_Audio_StopPlayback:i:i
void PPB_Audio_StopPlayback(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_StopPlayback(%d)\n", handle);
  DataAudio* data = GlobalAudioDataResources.GetDataForHandle(handle);
  CHECK(data != NULL);
  GlobalMultiMediaInterface->AudioStop();
  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_Audio API
// PP_Bool StopPlayback(PP_Resource audio);
// PPB_Audio_StartPlayback:i:i
void PPB_Audio_StartPlayback(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_StartPlayback(%d)\n", handle);
  DataAudio* data = GlobalAudioDataResources.GetDataForHandle(handle);
  CHECK(data != NULL);
  GlobalMultiMediaInterface->AudioStart();

  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_AudioConfig API
// PP_Resource CreateStereo16Bit(PP_Instance instance,
//                               PP_AudioSampleRate sample_rate,
//                               uint32_t sample_frame_count);
// PPB_AudioConfig_CreateStereo16Bit:iii:i
void PPB_AudioConfig_CreateStereo16Bit(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  int sample_frequency = ins[1]->u.ival;
  int sample_frame_count = ins[2]->u.ival;
  NaClLog(1, "PPB_AudioConfig_CreateStereo16Bit(%d, %d, %d)\n",
          instance, sample_frequency, sample_frame_count);

  const int handle = GlobalAudioConfigDataResources.Alloc();
  DataAudioConfig* data =
    GlobalAudioConfigDataResources.GetDataForHandle(handle);
  data->sample_frequency = sample_frequency;
  data->sample_frame_count = sample_frame_count;

  CHECK(sample_frame_count * kBytesPerSample < kMaxAudioBufferSize);
  outs[0]->u.ival = handle;
  NaClLog(1, "PPB_AudioConfig_CreateStereo16Bit -> %d\n", handle);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_AudioConfig_IsAudioConfig:i:i
// PP_Bool IsAudioConfig(PP_Resource resource);
void PPB_AudioConfig_IsAudioConfig(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_AudioConfig_IsAudioConfig(%d)\n", handle);
  DataAudioConfig* data =
    GlobalAudioConfigDataResources.GetDataForHandle(handle);
  // for now be strict to catch problems early
  CHECK(data != NULL);
  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_AudioConfig_RecommendSampleFrameCount:ii:i
// uint32_t RecommendSampleFrameCount(PP_AudioSampleRate sample_rate,
//                                    uint32_t requested_sample_frame_count);
void PPB_AudioConfig_RecommendSampleFrameCount(SRPC_PARAMS) {
  int sample_frequency = ins[0]->u.ival;
  int sample_frame_count = ins[1]->u.ival;
  NaClLog(LOG_INFO, "PPB_AudioConfig_RecommendSampleFrameCount(%d, %d)\n",
          sample_frequency, sample_frame_count);
  // This is clearly imperfect.
  // TODO(robertm): Consider using SDL's negotiation mechanism here
  outs[0]->u.ival = kRecommendSampleFrameCount;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_AudioConfig_GetSampleRate:i:i
// PP_AudioSampleRate GetSampleRate(PP_Resource config);
void PPB_AudioConfig_GetSampleRate(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_AudioConfig_GetSampleRate(%d)\n", handle);
  DataAudioConfig* data =
    GlobalAudioConfigDataResources.GetDataForHandle(handle);
  outs[0]->u.ival = data->sample_frequency;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_AudioConfig_GetSampleFrameCount:i:i
// uint32_t GetSampleFrameCount(PP_Resource config);
void PPB_AudioConfig_GetSampleFrameCount(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_AudioConfig_GetSampleFrameCount(%d)\n", handle);
  DataAudioConfig* data =
    GlobalAudioConfigDataResources.GetDataForHandle(handle);
  outs[0]->u.ival = data->sample_frame_count;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

}  //  namespace

void InvokeAudioStreamCreatedCallback(NaClCommandLoop* ncl,
                                      const UserEvent *event) {
  const int handle = event->result;
  DataAudio* data = GlobalAudioDataResources.GetDataForHandle(handle);
  DataAudioConfig* config =
    GlobalAudioConfigDataResources.GetDataForHandle(data->handle_config);
  NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* ins[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* outs[NACL_SRPC_MAX_ARGS + 1];
  BuildArgVec(outs, out, 0);
  BuildArgVec(ins, in, 4);
  ins[0]->tag = NACL_SRPC_ARG_TYPE_INT;
  ins[0]->u.ival = handle;
  ins[1]->tag = NACL_SRPC_ARG_TYPE_HANDLE;
  ins[1]->u.hval = data->desc_shmem->desc();
  ins[2]->tag = NACL_SRPC_ARG_TYPE_INT;
  ins[2]->u.ival = config->sample_frame_count * kBytesPerSample;
  ins[3]->tag = NACL_SRPC_ARG_TYPE_HANDLE;
  ins[3]->u.hval = data->desc_sync_out->desc();
  ncl->InvokeNexeRpc("PPP_Audio_StreamCreated:ihih:", ins, outs);
}


#define TUPLE(a, b) #a #b, a
void PepperEmuInitAudio(NaClCommandLoop* ncl, IMultimedia* im) {
  GlobalMultiMediaInterface = im;
  NaClLog(LOG_INFO, "PepperEmuInitAudio\n");

  ncl->AddUpcallRpc(TUPLE(PPB_Audio_Create, :ii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Audio_IsAudio, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Audio_GetCurrentConfig, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Audio_StopPlayback, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Audio_StartPlayback, :i:i));

  ncl->AddUpcallRpc(TUPLE(PPB_AudioConfig_CreateStereo16Bit, :iii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_AudioConfig_IsAudioConfig, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_AudioConfig_RecommendSampleFrameCount, :ii:i));
  ncl->AddUpcallRpc(TUPLE(PPB_AudioConfig_GetSampleRate, :i:i));
  ncl->AddUpcallRpc(TUPLE(PPB_AudioConfig_GetSampleFrameCount, :i:i));
}
