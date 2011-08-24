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

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/pp_size.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"


#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/primitives.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/pepper_emu.h"

// ======================================================================
const int kInvalidInstance = 0;
const int kInvalidHandle = 0;
// We currently have no bookkeeping for events, so this resource handle is
// used for all of them.
const int kFirstEventHandle = 6000;

// ======================================================================

// Note: Just a bunch of fairly unrelated global variables,
// we expect them to be zero initialized.
static struct {
  int instance;

  // for event loggging and replay
  std::ofstream event_logger_stream;
  std::ifstream event_replay_stream;
  std::queue<UserEvent*> events_ready_to_go;
  UserEvent* next_sync_event;

  IMultimedia* sdl_engine;
  std::string title;

  std::string quit_message;
} Global;

// ======================================================================
bool HandlerPepperEmuInitialize(NaClCommandLoop* ncl,
                                const vector<string>& args) {
  NaClLog(LOG_INFO, "HandlerSDLInitialize\n");
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  UNREFERENCED_PARAMETER(ncl);
  Global.instance = ExtractInt32(args[1]);
  Global.title = args[4];

  // NOTE: we decide at linktime which incarnation to use here
  Global.sdl_engine = MakeEmuPrimitives(ExtractInt32(args[2]),
                                        ExtractInt32(args[3]),
                                        Global.title.c_str());
  PepperEmuInitAudio(ncl, Global.sdl_engine);
  PepperEmuInitCore(ncl, Global.sdl_engine);
  PepperEmuInitFileIO(ncl, Global.sdl_engine);
  PepperEmuInitPostMessage(ncl, Global.sdl_engine);
  PepperEmuInit3D(ncl, Global.sdl_engine);
  PepperEmuInit2D(ncl, Global.sdl_engine);
  return true;
}


static UserEvent* GetNextEvent(bool poll) {
  UserEvent* event = NULL;
  if (Global.event_replay_stream.is_open()) {
    if (Global.events_ready_to_go.size() > 0) {
      // empty queue while we have ready to events
      event = Global.events_ready_to_go.front();
      Global.events_ready_to_go.pop();
      return event;
    } else if (Global.next_sync_event != NULL) {
      // wait for the matching sync event
      event = Global.sdl_engine->EventGet();

      if (IsTerminationEvent(event)) return event;

      // drop all regular events on the floor;
      if (IsInputEvent(event)) return NULL;

      // NOTE: we only replay the recorded input events.
      // Recorded UserEvents are used for synchronization with the
      // actual UserEvents that the system generates.
      // TODO(robertm): We may need to refine this because, in theory,
      // there is no guaranteed time ordering on the UserEvents.
      // One solution would be to only use the screen refresh
      // UserEvents (CUSTOM_EVENT_FLUSH_CALLBACK) as sync events and
      // ignore all others.
      // We can delay this work until we see the check below firing.
      CHECK(event->type == Global.next_sync_event->type);
      // sync event has been "consumed"
      Global.next_sync_event = NULL;
      return event;
    } else {
      // refill queue
      if (Global.event_replay_stream.eof()) {
        NaClLog(LOG_INFO, "replay events depleted\n");
        Global.events_ready_to_go.push(MakeTerminationEvent());
        return NULL;
      }
      while (true) {
        event = new UserEvent;
        Global.event_replay_stream.read(
          reinterpret_cast<char*>(event), sizeof(*event));
        if (Global.event_replay_stream.fail()) return NULL;
        CHECK(!IsInvalidEvent(event));
        if (!IsInputEvent(event)) {
          Global.next_sync_event = event;
          return false;
        } else {
          Global.events_ready_to_go.push(event);
        }
      }
    }
  } else {
    if (poll) {
      return Global.sdl_engine->EventPoll();
    } else {
      return Global.sdl_engine->EventGet();
    }
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


static bool HandleSynthesizedEvent(NaClCommandLoop* ncl, UserEvent* event) {
  NaClLog(1, "Got sythesized event [%s]\n",
          StringifyEvent(event).c_str());

  switch (event->type) {
    case EVENT_TYPE_TERMINATION:
      NaClLog(LOG_INFO, "Got termination event\n");
      return false;

    // This event is created on behalf of PPB_URLLoader_Open
    case EVENT_TYPE_OPEN_CALLBACK:
    // FALL THROUGH
    // This event is created on behalf of PPB_Core_CallOnMainThread
    case EVENT_TYPE_TIMER_CALLBACK:
    // FALL THROUGH
    // This event is created on behalf of PPB_URLLoader_ReadResponseBody
    case EVENT_TYPE_READ_CALLBACK:
    // FALL THROUGH
    // This event gets created so that we can invoke
    // RunCompletionCallback after PPB_Graphics2D_Flush
    case EVENT_TYPE_FLUSH_CALLBACK: {
      InvokeCompletionCallback(ncl,
                               event->callback,
                               event->result,
                               event->pointer,
                               event->size);
      return true;
    }
    // This event gets created so that we can invoke
    // PPP_Audio_StreamCreated after PPB_Audio_Create
    case EVENT_TYPE_INIT_AUDIO:
#if (NACL_LINUX || NACL_OSX)
      sleep(1);
#elif NACL_WINDOWS
      Sleep(1 * 1000);
#else
#error "Please specify platform as NACL_LINUX, NACL_OSX or NACL_WINDOWS"
#endif
      InvokeAudioStreamCreatedCallback(ncl, event);
      return true;
    case EVENT_TYPE_INPUT:
    case EVENT_TYPE_INVALID:
    default:
      NaClLog(LOG_FATAL, "unknown event type %d\n", event->type);
      return false;
  }
}

// The event loop below see two kinds of events. Regular input events
// such as keyboard and mouse events. And synthetic events that got injected
// into the event queue to trigger callbacks.
// The input events are forwarded to the nexe.
// The synthetic events are converted to the corresponding callback invokation.
//
// Having callbacks and input events going through the same queue has some
// advantages. In particular we can use the non-input events
// as synchronization markers (e.g. sync on every screen update) in the replay
// case.
bool HandlerPepperEmuEventLoop(NaClCommandLoop* ncl,
                               const vector<string>& args) {
  NaClLog(LOG_INFO, "HandlerPepperEmuEventLoop\n");
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "Insufficient arguments to 'rpc' command.\n");
    return false;
  }

  const bool poll = ExtractInt32(args[1]) != 0;
  const int msecs = ExtractInt32(args[2]);

  NaClLog(LOG_INFO, "Entering event loop. Polling: %d  duration_ms: %d\n",
          poll, msecs);

  int64_t termination_time = NaClGetTimeOfDayMicroseconds() + msecs * 1000;
  bool keep_going = true;
  while (keep_going && NaClGetTimeOfDayMicroseconds() < termination_time) {
    NaClLog(2, "Pepper emu event loop wait\n");
    UserEvent* event = GetNextEvent(poll);
    if (event == NULL) {
      continue;
    }

    if (Global.event_logger_stream.is_open() && !IsInvalidEvent(event)) {
      Global.event_logger_stream.write(reinterpret_cast<char*>(event),
                                       sizeof(*event));
    }

    if (IsInputEvent(event)) {
      // NOTE: for now always use the same event resource
      InvokeInputEventCallback(ncl, event, Global.instance, kFirstEventHandle);
    } else {
      keep_going = HandleSynthesizedEvent(ncl, event);
    }
    // NOTE: this is the global event sink where all events get deleted.
    delete event;
  }

  NaClLog(LOG_INFO, "Exiting event loop (%s)\n",
          keep_going ? "timeout" : "termination");
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
  Global.next_sync_event = NULL;
  Global.event_replay_stream.open(filename.c_str(),
                                  std::ios::in | std::ios::binary);
  if (!Global.event_replay_stream.is_open()) {
    NaClLog(LOG_FATAL, "Cannot open %s\n", filename.c_str());
  }
}
