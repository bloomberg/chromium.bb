/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_H_

/* Abstract multimedia interface */
/* NOTE: currently only video is supported */

union NaClMultimediaEvent;

class IMultimedia {
 public:
  virtual ~IMultimedia() {}

  virtual int VideoBufferSize() = 0;
  virtual void VideoUpdate(const void* data) = 0;
  virtual void EventPoll(NaClMultimediaEvent* event) = 0;
};

/* Currently, there is only an SDL implementation */
IMultimedia* MakeMultimediaSDL(int width, int heigth, const char* title);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERASAL_MULTIMEDIA_H_ */
