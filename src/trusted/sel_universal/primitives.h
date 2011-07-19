/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_PRIMITIVES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_PRIMITIVES_H_

#include "native_client/src/third_party/ppapi/c/pp_input_event.h"

// ppapi does not have a user defined event notion.
// c.f. ppapi/c/pp_input_event.h
// So we superimpose one here on the existing one "u" member.

enum {
  CUSTOM_EVENT_START = 2000,
  CUSTOM_EVENT_OPEN_CALLBACK,
  CUSTOM_EVENT_TERMINATION,
  CUSTOM_EVENT_FLUSH_CALLBACK,
  CUSTOM_EVENT_INIT_AUDIO,
  CUSTOM_EVENT_TIMER_CALLBACK,
  CUSTOM_EVENT_READ_CALLBACK
};

struct PP_InputEvent_User {
  int callback;   // this is often the handle for a callback on the nexe side
  int result;     // this is often the result for the callback
  void* pointer;  // this is often a marshalled pp_var
  int size;       // size of the marshalled pp_var
};


// User Event Helpers
bool IsUserEvent(PP_InputEvent* event);

void MakeUserEvent(PP_InputEvent* event,
                   int code,
                   int callback,
                   int result,
                   void* pointer,
                   int size);
void MakeInvalidEvent(PP_InputEvent* event);
void MakeTerminationEvent(PP_InputEvent* event);

bool IsInvalidEvent(PP_InputEvent* event);
bool IsTerminationEvent(PP_InputEvent* event);

// NOTE: this returns an int rather than PP_InputEvent_Type
int GetUserEventType(PP_InputEvent* event);

PP_InputEvent_User* GetUserEvent(PP_InputEvent* event);

typedef void (*AUDIO_CALLBACK)(void* data, unsigned char* buffer, int length);

class IMultimedia {
 public:
  virtual ~IMultimedia() {}

  virtual int VideoBufferSize() = 0;
  virtual int VideoWidth() = 0;
  virtual int VideoHeight() = 0;
  // Trigger a frame update in the multimedia system
  virtual void VideoUpdate(const void* data) = 0;
  // Inject a userevent into the event buffer.
  // NOTE: the event will be copied
  virtual void PushUserEvent(PP_InputEvent* event) = 0;

  // schedule a event for future delivery
  virtual void PushDelayedUserEvent(int delay, PP_InputEvent* event) = 0;
  // Get next event (non-blocking). If no event was available
  // IsInvalidEvent(event) will be true afterwards.
  virtual void EventPoll(PP_InputEvent* event) = 0;
  // Get next event (blocking).
  virtual void EventGet(PP_InputEvent* event) = 0;
  // Get next event (blocking).
  virtual void AudioInit16Bit(int frequency,
                              int channels,
                              int frame_count,
                              AUDIO_CALLBACK cb,
                              void* cb_data) = 0;
  virtual void AudioStart() = 0;
  virtual void AudioStop() = 0;
};

/* Currently, there is only an SDL implementation */
IMultimedia* MakeEmuPrimitives(int width, int heigth, const char* title);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_PRIMITIVES_H_ */
