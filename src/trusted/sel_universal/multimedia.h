/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_H_

// Abstract multimedia interface
// NOTE: currently only video and simple events are supported

class PP_InputEvent;

const int MY_EVENT_FLUSH_CALL_BACK = 88;
const int MY_EVENT_INIT_AUDIO = 89;
// data1 = callback_id, data2 = result value
const int MY_EVENT_TIMER_CALL_BACK = 90;



bool IsUserEvent(PP_InputEvent* event);
void MakeUserEvent(PP_InputEvent* event, int code, int data1, int data2);

bool IsInvalidEvent(PP_InputEvent* event);
void MakeInvalidEvent(PP_InputEvent* event);

bool IsTerminationEvent(PP_InputEvent* event);
void MakeTerminationEvent(PP_InputEvent* event);


int GetCodeFromUserEvent(PP_InputEvent* event);
int GetData1FromUserEvent(PP_InputEvent* event);
int GetData2FromUserEvent(PP_InputEvent* event);

void MakeInvalidEvent(PP_InputEvent* event);

typedef void (*AUDIO_CALLBACK)(void* data, unsigned char* buffer, int length);

class IMultimedia {
 public:
  virtual ~IMultimedia() {}

  virtual int VideoBufferSize() = 0;
  // Trigger a frame update in the multimedia system
  virtual void VideoUpdate(const void* data) = 0;
  // Inject a userevent with the two arbitrary pieces of
  // data into the event buffer
  virtual void PushUserEvent(int delay, int code, int data1, int data2) = 0;
  // Get next event (non-blocking). If no event was available
  // IsInvalidEvent(event) will be true afterwards.
  virtual void EventPoll(PP_InputEvent* event) = 0;
  // Get next event (blocking).
  virtual void EventGet(PP_InputEvent* event) = 0;
  // Get next event (blocking).
  virtual void AudioInit16Bit(int frequency,
                              int channels,
                              int frame_count,
                              AUDIO_CALLBACK cb) = 0;
  virtual void AudioStart() = 0;
  virtual void AudioStop() = 0;
};

/* Currently, there is only an SDL implementation */
IMultimedia* MakeMultimediaSDL(int width, int heigth, const char* title);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_H_ */
