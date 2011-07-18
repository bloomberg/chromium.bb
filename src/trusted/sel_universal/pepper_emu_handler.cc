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
const int kFirstAudioHandle = 300;
const int kFirstAudioConfigHandle = 400;
const int kBytesPerSample = 4;  // 16-bit stereo

const int kRecommendSampleFrameCount = 2048;
const int kMaxAudioBufferSize = 0x10000;

// ======================================================================

// Note: Just a bunch of fairly unrelated global variables,
// we expect them to be zero initialized.
static struct {
  int instance;

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

  Global.instance = ExtractInt32(args[1]);
  Global.title = args[4];

  // NOTE: we decide at linktime which incarnation to use here
  Global.sdl_engine = MakeEmuPrimitives(ExtractInt32(args[2]),
                                        ExtractInt32(args[3]),
                                        Global.title.c_str());
  PepperEmuInitCore(ncl, Global.sdl_engine);
  PepperEmuInitFileIO(ncl, Global.sdl_engine);
  PepperEmuInitPostMessage(ncl, Global.sdl_engine);
  PepperEmuInit3D(ncl, Global.sdl_engine);
  PepperEmuInit2D(ncl, Global.sdl_engine);
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
