/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_PRIMITIVES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_PRIMITIVES_H_

#include <string>
enum EVENT_TYPE {
  EVENT_TYPE_INVALID,
  EVENT_TYPE_OPEN_CALLBACK,
  EVENT_TYPE_TERMINATION,
  EVENT_TYPE_FLUSH_CALLBACK,
  EVENT_TYPE_INIT_AUDIO,
  EVENT_TYPE_TIMER_CALLBACK,
  EVENT_TYPE_READ_CALLBACK,
  EVENT_TYPE_INPUT
};

// NOTE: the first element has to be the type field so we can
//       distinguish it from other event structs given that they
//       also start with a such a field.
struct UserEvent {
  EVENT_TYPE type;
  int callback;   // this is often the callback handle on the nexe side
  int result;     // this is often the result for the callback
  void* pointer;  // this is often a marshalled pp_var
  int size;       // size of the marshalled pp_var
};

// User Event Helpers
bool IsInputEvent(const UserEvent* event);
bool IsInvalidEvent(const UserEvent* event);
bool IsTerminationEvent(const UserEvent* event);

UserEvent* MakeUserEvent(EVENT_TYPE type,
                         int callback,
                         int result,
                         void* pointer,
                         int size);
UserEvent* MakeInvalidEvent();
UserEvent* MakeTerminationEvent();
// for debugging
std::string StringifyEvent(const UserEvent* event);
std::string StringifyEventType(const UserEvent* event);

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
  virtual void PushUserEvent(UserEvent* event) = 0;

  // schedule a event for future delivery
  virtual void PushDelayedUserEvent(int delay, UserEvent* event) = 0;
  // Get next event (non-blocking). If no event was available
  // IsInvalidEvent(event) will be true afterwards.
  virtual UserEvent* EventPoll() = 0;
  // Get next event (blocking).
  virtual UserEvent* EventGet() = 0;
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
