/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This file exports a single function used to setup the
// multimedia sub-system for use with sel_universal
// It was inpspired by src/trusted/plugin/srpc/multimedia_socket.cc
// On the untrusted side it interface with: src/untrusted/av/nacl_av.c
//
// NOTE: this is experimentation and testing. We are not concerned
//       about descriptor and memory leaks

#include <string.h>
#include <fstream>
#include <queue>
#include <string>

#if (NACL_LINUX)
// for shmem
#include <sys/ipc.h>
#include <sys/shm.h>
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
// for sync_sockets
#include <sys/socket.h>
#endif


#include "native_client/src/third_party/ppapi/c/pp_errors.h"
#include "native_client/src/third_party/ppapi/c/pp_input_event.h"
#include "native_client/src/third_party/ppapi/c/pp_size.h"
#include "native_client/src/third_party/ppapi/c/ppb_audio.h"
#include "native_client/src/third_party/ppapi/c/ppb_audio_config.h"
#include "native_client/src/third_party/ppapi/c/ppb_image_data.h"

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"

#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"
#include "native_client/src/trusted/sel_universal/srpc_helper.h"

using nacl::DescWrapperFactory;
using nacl::DescWrapper;

// ======================================================================
const int kInvalidInstance = 0;
const int kInvalidHandle = 0;
const int kFirstImageDataHandle = 100;
const int kFirstGraphicsHandle = 200;
const int kFirstAudioHandle = 300;
const int kFirstAudioConfigHandle = 400;
const int kBytesPerSample = 4;  // 16-bit stereo

const int kRecommendSampleFrameCount = 2048;
const int kMaxAudioBufferSize = 0x10000;
const int kBytesPerPixel = 4;

// ======================================================================

// Note: Just a bunch of fairly unrelated global variables,
// we expect them to be zero initialized.
static struct {
  int instance;

  // video stuff
  int screen_width;
  int screen_height;

  int handle_graphics;
  int handle_image_data;
  int image_data_size;

  nacl::DescWrapper* desc_video_shmem;
  void* addr_video;

  // audio stuff
  int handle_audio;
  int handle_audio_config;

  int sample_frequency;
  int sample_frame_count;

  nacl::DescWrapper* desc_audio_shmem;
  nacl::DescWrapper* desc_audio_sync_in;
  nacl::DescWrapper* desc_audio_sync_out;
  void* addr_audio;

  // for event loggging and replay
  std::ofstream event_logger_stream;
  std::ifstream event_replay_stream;
  std::queue<PP_InputEvent> events_ready_to_go;
  PP_InputEvent next_sync_event;

  IMultimedia* sdl_engine;
  std::string title;

  std::string quit_message;
} Global;

// ======================================================================
static void AudioCallBack(void* data, unsigned char* buffer, int length) {
  NaClLog(2, "AudioCallBack(%p, %p, %d)\n", data, buffer, length);
  UNREFERENCED_PARAMETER(data);
  CHECK(length == Global.sample_frame_count * kBytesPerSample);
  // NOTE: we copy the previously filled buffer.
  //       This introduces extra latency but simplifies the design
  //       as we do not have to wait for the nexe to generate the data.
  memcpy(buffer, Global.addr_audio, length);

  // ping sync socket
  int value = 0;
  Global.desc_audio_sync_in->Write(&value, sizeof value);
}

// From the ImageData API
// PP_Resource Create(PP_Instance instance,
//                    PP_ImageDataFormat format,
//                    const struct PP_Size* size,
//                    PP_Bool init_to_zero);
// PPB_ImageData_Create:iiCi:i
//
// TODO(robertm) this function can currently be called only once
//               and the dimension must match the global values
//               and the format is fixed.
static void PPB_ImageData_Create(SRPC_PARAMS) {
  const int instance = ins[0]->u.ival;
  const int format = ins[1]->u.ival;
  CHECK(ins[2]->u.count == sizeof(PP_Size));
  PP_Size* img_size = (PP_Size*) ins[2]->arrays.carr;
  NaClLog(1, "PPB_ImageData_Create(%d, %d, %d, %d)\n",
          instance, format, img_size->width, img_size->height);

  CHECK(Global.handle_image_data == kInvalidHandle);
  Global.handle_image_data = kFirstImageDataHandle;
  CHECK(Global.instance != kInvalidInstance);
  CHECK(instance == Global.instance);
  CHECK(format == PP_IMAGEDATAFORMAT_BGRA_PREMUL);

  CHECK(Global.screen_width == img_size->width);
  CHECK(Global.screen_height == img_size->height);
  Global.image_data_size = kBytesPerPixel * img_size->width * img_size->height;

  nacl::DescWrapperFactory factory;
  Global.desc_video_shmem = factory.MakeShm(Global.image_data_size);
  size_t dummy_size;

  if (Global.desc_video_shmem->Map(&Global.addr_video, &dummy_size)) {
    NaClLog(LOG_FATAL, "cannot map video shmem\n");
  }

  if (ins[3]->u.ival) {
    memset(Global.addr_video, 0, Global.image_data_size);
  }

  outs[0]->u.ival = Global.handle_image_data;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the ImageData API
// PP_Bool Describe(PP_Resource image_data,
//                  struct PP_ImageDataDesc* desc);
// PPB_ImageData_Describe:i:Chii
static void PPB_ImageData_Describe(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_ImageData_Describe(%d)\n", handle);
  CHECK(handle == Global.handle_image_data);

  PP_ImageDataDesc d;
  d.format = PP_IMAGEDATAFORMAT_BGRA_PREMUL;
  d.size.width =  Global.screen_width;
  d.size.height = Global.screen_height;
  // we handle only rgba data -> each pixel is 4 bytes.
  d.stride = Global.screen_width * kBytesPerPixel;
  outs[0]->u.count = sizeof(d);
  outs[0]->arrays.carr = reinterpret_cast<char*>(calloc(1, sizeof(d)));
  memcpy(outs[0]->arrays.carr, &d, sizeof(d));

  outs[1]->u.hval = Global.desc_video_shmem->desc();
  outs[2]->u.ival = Global.image_data_size;
  outs[3]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// PP_Resource Create(PP_Instance instance,
//                    const struct PP_Size* size,
//                    PP_Bool is_always_opaque);
// PPB_Graphics2D_Create:iCi:i
//
// TODO(robertm) This function can currently be called only once
//               The size must be the same as the one provided via
//                HandlerSDLInitialize()
static void PPB_Graphics2D_Create(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  NaClLog(1, "PPB_Graphics2D_Create(%d)\n", instance);
  CHECK(Global.handle_graphics == kInvalidHandle);
  Global.handle_graphics = kFirstGraphicsHandle;
  CHECK(instance == Global.instance);
  PP_Size* img_size = reinterpret_cast<PP_Size*>(ins[1]->arrays.carr);
  CHECK(Global.screen_width == img_size->width);
  CHECK(Global.screen_height == img_size->height);
  // TODO(robertm):  is_always_opaque is currently ignored
  outs[0]->u.ival = Global.handle_graphics;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PP_Bool BindGraphics(PP_Instance instance, PP_Resource device);
// PPB_Instance_BindGraphics:ii:i
static void PPB_Instance_BindGraphics(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  int handle = ins[1]->u.ival;
  NaClLog(1, "PPB_Instance_BindGraphics(%d, %d)\n",
          instance, handle);
  CHECK(instance == Global.instance);
  CHECK(handle == Global.handle_graphics);
  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// void ReplaceContents(PP_Resource graphics_2d,
//                      PP_Resource image_data);
// PPB_Graphics2D_ReplaceContents:ii:
//
// NOTE: this is completely ignored and we postpone all action to "Flush"
static void PPB_Graphics2D_ReplaceContents(SRPC_PARAMS) {
  int handle_graphics = ins[0]->u.ival;
  int handle_image_data = ins[1]->u.ival;
  UNREFERENCED_PARAMETER(outs);
  NaClLog(1, "PPB_Graphics2D_ReplaceContents(%d, %d)\n",
          handle_graphics, handle_image_data);
  CHECK(handle_graphics == Global.handle_graphics);
  CHECK(handle_image_data == Global.handle_image_data);

  // For now assume this will be immediately followed by a Flush
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// void PaintImageData(PP_Resource graphics_2d,
//                     PP_Resource image_data,
//                     const struct PP_Point* top_left,
//                     const struct PP_Rect* src_rect);
// PPB_Graphics2D_PaintImageData:iiCC:
//
// NOTE: this is completely ignored and we postpone all action to "Flush"
//       Furhermore we assume that entire image is painted
static void PPB_Graphics2D_PaintImageData(SRPC_PARAMS) {
  int handle_graphics = ins[0]->u.ival;
  int handle_image_data = ins[1]->u.ival;
  UNREFERENCED_PARAMETER(outs);
  NaClLog(1, "PPB_Graphics2D_PaintImageData(%d, %d)\n",
          handle_graphics, handle_image_data);
  CHECK(handle_graphics == Global.handle_graphics);
  CHECK(handle_image_data == Global.handle_image_data);

  // For now assume this will be immediately followed by a Flush
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the Graphics2D API
// int32_t Flush(PP_Resource graphics_2d,
//               struct PP_CompletionCallback callback);
// PPB_Graphics2D_Flush:ii:i
static void PPB_Graphics2D_Flush(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  int callback_id = ins[1]->u.ival;
  NaClLog(1, "PPB_Graphics2D_Flush(%d, %d)\n", handle, callback_id);
  CHECK(handle == Global.handle_graphics);
  outs[0]->u.ival = -1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);

  Global.sdl_engine->VideoUpdate(Global.addr_video);
  NaClLog(1, "pushing user event for callback (%d)\n", callback_id);
  PP_InputEvent event;
  MakeUserEvent(&event, CUSTOM_EVENT_FLUSH_CALLBACK, callback_id, 0, 0, 0);
  Global.sdl_engine->PushUserEvent(&event);
}

// From the PPB_Audio API
// PP_Resource Create(PP_Instance instance, PP_Resource config,
//                    PPB_Audio_Callback audio_callback, void* user_data);
// PPB_Audio_Create:ii:i
static void PPB_Audio_Create(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  int handle = ins[1]->u.ival;
  NaClLog(1, "PPB_Audio_Create(%d, %d)\n", instance, handle);
  CHECK(instance == Global.instance);
  CHECK(handle == Global.handle_audio_config);

  nacl::DescWrapperFactory factory;
#if (NACL_WINDOWS || NACL_OSX)
  NaClLog(LOG_ERROR, "HandlerSyncSocketCreate NYI for windows/mac\n");
#else
  nacl::Handle handles[2] = {nacl::kInvalidHandle, nacl::kInvalidHandle};
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, handles) != 0) {
    NaClLog(LOG_FATAL, "cannot create syn sockets\n");
  }
  Global.desc_audio_sync_in = factory.ImportSyncSocketHandle(handles[0]);
  Global.desc_audio_sync_out = factory.ImportSyncSocketHandle(handles[1]);
#endif
  Global.desc_audio_shmem = factory.MakeShm(kMaxAudioBufferSize);
  size_t dummy_size;

  if (Global.desc_audio_shmem->Map(&Global.addr_audio, &dummy_size)) {
    NaClLog(LOG_FATAL, "cannot map audio shmem\n");
  }
  NaClLog(LOG_INFO, "PPB_Audio_Create: buffer is %p\n", Global.addr_audio);

  Global.sdl_engine->AudioInit16Bit(Global.sample_frequency,
                                    2,
                                    Global.sample_frame_count,
                                    &AudioCallBack);

  CHECK(Global.handle_audio == kInvalidHandle);
  Global.handle_audio = kFirstAudioHandle;
  outs[0]->u.ival = Global.handle_audio;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);

  PP_InputEvent event;
  MakeUserEvent(&event, CUSTOM_EVENT_INIT_AUDIO, 0, 0, 0, 0);
  Global.sdl_engine->PushUserEvent(&event);
}

// From the PPB_Audio API
// PP_Bool IsAudio(PP_Resource resource)
// PPB_Audio_IsAudio:i:i
static void PPB_Audio_IsAudio(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_IsAudio(%d)\n", handle);
  CHECK(handle == Global.handle_audio);

  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_Audio API
// PP_Resource GetCurrentConfig(PP_Resource audio);
// PPB_Audio_GetCurrentConfig:i:i
static void PPB_Audio_GetCurrentConfig(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_GetCurrentConfig(%d)\n", handle);
  CHECK(handle == Global.handle_audio);
  CHECK(Global.handle_audio_config != kInvalidHandle);

  outs[0]->u.ival = Global.handle_audio_config;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_Audio API
// PP_Bool StartPlayback(PP_Resource audio);
// PPB_Audio_StopPlayback:i:i
static void PPB_Audio_StopPlayback(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_StopPlayback(%d)\n", handle);
  CHECK(handle == Global.handle_audio);
  Global.sdl_engine->AudioStop();

  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_Audio API
// PP_Bool StopPlayback(PP_Resource audio);
// PPB_Audio_StartPlayback:i:i
static void PPB_Audio_StartPlayback(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_Audio_StartPlayback(%d)\n", handle);
  CHECK(handle == Global.handle_audio);
  Global.sdl_engine->AudioStart();

  outs[0]->u.ival = 1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// From the PPB_AudioConfig API
// PP_Resource CreateStereo16Bit(PP_Instance instance,
//                               PP_AudioSampleRate sample_rate,
//                               uint32_t sample_frame_count);
// PPB_AudioConfig_CreateStereo16Bit:iii:i
static void PPB_AudioConfig_CreateStereo16Bit(SRPC_PARAMS) {
  int instance = ins[0]->u.ival;
  Global.sample_frequency = ins[1]->u.ival;
  Global.sample_frame_count = ins[2]->u.ival;
  NaClLog(1, "PPB_AudioConfig_CreateStereo16Bit(%d, %d, %d)\n",
          instance, Global.sample_frequency, Global.sample_frame_count);
  CHECK(instance == Global.instance);
  CHECK(Global.sample_frame_count * kBytesPerSample < kMaxAudioBufferSize);
  CHECK(Global.handle_audio_config == kInvalidHandle);
  Global.handle_audio_config = kFirstAudioConfigHandle;
  outs[0]->u.ival = Global.handle_audio_config;

  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_AudioConfig_IsAudioConfig:i:i
// PP_Bool IsAudioConfig(PP_Resource resource);
static void PPB_AudioConfig_IsAudioConfig(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_AudioConfig_IsAudioConfig(%d)\n", handle);
  outs[0]->u.ival = (handle == Global.handle_audio_config);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_AudioConfig_RecommendSampleFrameCount:ii:i
// uint32_t RecommendSampleFrameCount(PP_AudioSampleRate sample_rate,
//                                    uint32_t requested_sample_frame_count);
static void PPB_AudioConfig_RecommendSampleFrameCount(SRPC_PARAMS) {
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
static void PPB_AudioConfig_GetSampleRate(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_AudioConfig_GetSampleRate(%d)\n", handle);
  CHECK(handle == Global.handle_audio_config);

  outs[0]->u.ival = Global.sample_frequency;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

// PPB_AudioConfig_GetSampleFrameCount:i:i
// uint32_t GetSampleFrameCount(PP_Resource config);
static void PPB_AudioConfig_GetSampleFrameCount(SRPC_PARAMS) {
  int handle = ins[0]->u.ival;
  NaClLog(1, "PPB_AudioConfig_GetSampleFrameCount(%d)\n", handle);
  CHECK(handle == Global.handle_audio_config);

  outs[0]->u.ival = Global.sample_frame_count;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


// TODO(robertm): rename this to HandlerPepperEmuInitialize
#define TUPLE(a, b) #a #b, a
bool HandlerPepperEmuInitialize(NaClCommandLoop* ncl,
                                const vector<string>& args) {
  NaClLog(LOG_INFO, "HandlerSDLInitialize\n");
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  UNREFERENCED_PARAMETER(ncl);
  // TODO(robertm): factor the audio video rpcs into a different file
  // and initialize them via say PepperEmuInitAV()
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

  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_Create, :iCi:i));
  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_ReplaceContents, :ii:));
  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_PaintImageData, :iiCC:));
  ncl->AddUpcallRpc(TUPLE(PPB_Graphics2D_Flush, :ii:i));

  ncl->AddUpcallRpc(TUPLE(PPB_ImageData_Describe, :i:Chii));
  ncl->AddUpcallRpc(TUPLE(PPB_ImageData_Create, :iiCi:i));

  ncl->AddUpcallRpc(TUPLE(PPB_Instance_BindGraphics, :ii:i));


  Global.instance = ExtractInt32(args[1]);
  Global.screen_width = ExtractInt32(args[2]);
  Global.screen_height = ExtractInt32(args[3]);
  Global.title = args[4];

  // NOTE: we decide at linktime which incarnation to use here
  Global.sdl_engine = MakeEmuPrimitives(Global.screen_width,
                                        Global.screen_height,
                                        Global.title.c_str());
  PepperEmuInitCore(ncl, Global.sdl_engine);
  PepperEmuInitFileIO(ncl, Global.sdl_engine);
  PepperEmuInitPostMessage(ncl, Global.sdl_engine);
  return true;
}


bool GetNextEvent(PP_InputEvent* event) {
  if (Global.event_replay_stream.is_open()) {
    if (Global.events_ready_to_go.size() > 0) {
      // empty queue while we have ready to events
      *event = Global.events_ready_to_go.front();
      Global.events_ready_to_go.pop();
      return true;
    } else if (!IsInvalidEvent(&Global.next_sync_event)) {
      // wait for the matching sync event
      Global.sdl_engine->EventGet(event);

      if (IsTerminationEvent(event)) return true;

      // drop all regular events on the floor;
      if (!IsUserEvent(event)) return false;

      // NOTE: we only replay the recorded input events.
      // Recorded UserEvents are used for synchronization with the
      // actual UserEvents that the system generates.
      // TODO(robertm): We may need to refine this because, in theory,
      // there is no guaranteed time ordering on the UserEvents.
      // One solution would be to only use the screen refresh
      // UserEvents (CUSTOM_EVENT_FLUSH_CALLBACK) as sync events and
      // ignore all others.
      // We can delay this work until we see the check below firing.
      CHECK(event->type == Global.next_sync_event.type);
      // sync event has been "consumed"
      MakeInvalidEvent(&Global.next_sync_event);
      return true;
    } else {
      // refill queue
      if (Global.event_replay_stream.eof()) {
        NaClLog(LOG_INFO, "replay events depleted\n");
        MakeTerminationEvent(event);
        Global.events_ready_to_go.push(*event);
        return false;
      }
      while (true) {
        Global.event_replay_stream.read(
          reinterpret_cast<char*>(event), sizeof(*event));
        if (Global.event_replay_stream.fail()) return false;
        CHECK(!IsInvalidEvent(event));
        if (IsUserEvent(event)) {
          Global.next_sync_event = *event;
          return false;
        } else {
          Global.events_ready_to_go.push(*event);
        }
      }
    }
  } else {
#if defined(USE_POLLING)
    Global.sdl_engine->EventPoll(event);
    if (IsInvalidEvent(&event)) return false;
#else
    Global.sdl_engine->EventGet(event);
#endif
    return true;
  }
}


static void InvokeCompletionCallback(NaClCommandLoop* ncl,
                                     int callback,
                                     int result,
                                     void* data,
                                     int size) {
  NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* ins[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* outs[NACL_SRPC_MAX_ARGS + 1];
  BuildArgVec(outs, out, 0);
  BuildArgVec(ins, in, 3);

  int dummy_exception[2] = {0, 0};

  ins[0]->tag = NACL_SRPC_ARG_TYPE_INT;
  ins[0]->u.ival = callback;
  ins[1]->tag = NACL_SRPC_ARG_TYPE_INT;
  ins[1]->u.ival = result;
  ins[2]->tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
  if (data) {
    ins[2]->u.count = size;;
    ins[2]->arrays.carr = reinterpret_cast<char*>(data);
  } else {
    ins[2]->u.count = sizeof(dummy_exception);
    ins[2]->arrays.carr = reinterpret_cast<char*>(dummy_exception);
  }

  ncl->InvokeNexeRpc("RunCompletionCallback:iiC:", ins, outs);
}


static void InvokeHandleInputEvent(NaClCommandLoop* ncl,
                                   PP_InputEvent* event) {
  NaClLog(1, "Got input event of type %d\n", event->type);
  NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* ins[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* outs[NACL_SRPC_MAX_ARGS + 1];
  BuildArgVec(ins, in, 2);
  BuildArgVec(outs, out, 1);

  ins[0]->tag = NACL_SRPC_ARG_TYPE_INT;
  ins[0]->u.ival = Global.instance;
  ins[1]->tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
  ins[1]->u.count = sizeof(event);
  ins[1]->arrays.carr = reinterpret_cast<char*>(event);

  outs[0]->tag = NACL_SRPC_ARG_TYPE_INT;
  ncl->InvokeNexeRpc("PPP_Instance_HandleInputEvent:iC:i", ins, outs);
}


static void InvokeAudioStreamCreated(NaClCommandLoop* ncl) {
  NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* ins[NACL_SRPC_MAX_ARGS + 1];
  NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
  NaClSrpcArg* outs[NACL_SRPC_MAX_ARGS + 1];
  BuildArgVec(outs, out, 0);
  BuildArgVec(ins, in, 4);
  ins[0]->tag = NACL_SRPC_ARG_TYPE_INT;
  ins[0]->u.ival = Global.handle_audio;
  ins[1]->tag = NACL_SRPC_ARG_TYPE_HANDLE;
  ins[1]->u.hval = Global.desc_audio_shmem->desc();
  ins[2]->tag = NACL_SRPC_ARG_TYPE_INT;
  ins[2]->u.ival = Global.sample_frame_count * kBytesPerSample;
  ins[3]->tag = NACL_SRPC_ARG_TYPE_HANDLE;
  ins[3]->u.hval = Global.desc_audio_sync_out->desc();
  ncl->InvokeNexeRpc("PPP_Audio_StreamCreated:ihih:", ins, outs);
}

static bool HandleUserEvent(NaClCommandLoop* ncl, PP_InputEvent* event) {
  // NOTE: this hack is necessary because we are overlaying two different
  //       enums and gcc's range propagation is rather smart.
  const int event_type = GetUserEventType(event);
  NaClLog(1, "Got user event with code %d\n", event_type);
  PP_InputEvent_User *user_event = GetUserEvent(event);

  switch (event_type) {
   case CUSTOM_EVENT_TERMINATION:
    NaClLog(LOG_INFO, "Got termination event\n");
    return false;

    // This event is created on behalf of PPB_URLLoader_Open
   case CUSTOM_EVENT_OPEN_CALLBACK:
    // FALL THROUGH
    // This event is created on behalf of PPB_Core_CallOnMainThread
   case CUSTOM_EVENT_TIMER_CALLBACK:
    // FALL THROUGH
    // This event is created on behalf of PPB_URLLoader_ReadResponseBody
   case CUSTOM_EVENT_READ_CALLBACK:
    // FALL THROUGH
    // This event gets created so that we can invoke
    // RunCompletionCallback after PPB_Graphics2D_Flush
   case CUSTOM_EVENT_FLUSH_CALLBACK: {
     NaClLog(2, "Completion callback(%d, %d)\n",
             user_event->callback, user_event->result);
     InvokeCompletionCallback(ncl,
                              user_event->callback,
                              user_event->result,
                              user_event->pointer,
                              user_event->size);
     return true;
   }
    // This event gets created so that we can invoke
    // PPP_Audio_StreamCreated after PPB_Audio_Create
   case CUSTOM_EVENT_INIT_AUDIO:
    NaClLog(1, "audio init callback\n");
#if (NACL_LINUX || NACL_OSX)
    sleep(1);
#elif NACL_WINDOWS
    Sleep(1 * 1000);
#else
#error "Please specify platform as NACL_LINUX, NACL_OSX or NACL_WINDOWS"
#endif
    InvokeAudioStreamCreated(ncl);
    return true;

   default:
    NaClLog(LOG_FATAL, "unknown event type %d\n", event->type);
    return false;
  }
}

// uncomment the line below if you want to use a non-blocking
// event processing loop.
// This can be sometime useful for debugging but is wasting cycles
// #define USE_POLLING

bool HandlerPepperEmuEventLoop(NaClCommandLoop* ncl,
                               const vector<string>& args) {
  NaClLog(LOG_INFO, "HandlerSDLEventLoop\n");
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(ncl);
  PP_InputEvent event;
  bool keep_going = true;
  while (keep_going) {
    NaClLog(1, "Pepper emu event loop wait\n");
    if (!GetNextEvent(&event)) {
      continue;
    }

    if (Global.event_logger_stream.is_open() && !IsInvalidEvent(&event)) {
      Global.event_logger_stream.write(reinterpret_cast<char*>(&event),
                                       sizeof(event));
    }

    if (IsUserEvent(&event)) {
      keep_going = HandleUserEvent(ncl, &event);
    } else {
      InvokeHandleInputEvent(ncl, &event);
    }
  }
  NaClLog(LOG_INFO, "Exiting event loop\n");
  return true;
}


void RecordPPAPIEvents(std::string filename) {
  NaClLog(LOG_INFO, "recoding events to %s\n", filename.c_str());
  Global.event_logger_stream.open(
    filename.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
  if (!Global.event_logger_stream.is_open()) {
    NaClLog(LOG_FATAL, "Cannot open %s\n", filename.c_str());
  }
}


void ReplayPPAPIEvents(std::string filename) {
  NaClLog(LOG_INFO, "replaying events from %s\n", filename.c_str());
  MakeInvalidEvent(&Global.next_sync_event);
  Global.event_replay_stream.open(filename.c_str(),
                                  std::ios::in | std::ios::binary);
  if (!Global.event_replay_stream.is_open()) {
    NaClLog(LOG_FATAL, "Cannot open %s\n", filename.c_str());
  }
}
