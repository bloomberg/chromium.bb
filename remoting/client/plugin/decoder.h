// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(hclam): Enable this when we have VP8.
// extern "C" {
// #include "remoting/demo/third_party/on2/include/on2_decoder.h"
// #include "remoting/demo/third_party/on2/include/vp8dx.h"
// }

class Stream;

class VideoDecoder {
 public:
  virtual ~VideoDecoder() {}
  virtual bool DecodeFrame(char* buffer, int size) = 0;
  virtual bool GetDecodedFrame(char** planes, int* strides) = 0;
  virtual bool IsInitialized() = 0;
  virtual int GetWidth() = 0;
  virtual int GetHeight() = 0;
  virtual int GetFormat() = 0;
};

// TODO(hclam): Enable this when we have VP8.
// class VP8VideoDecoder {
//  public:
//   VP8VideoDecoder();
//   virtual bool DecodeFrame(char* buffer, int size);
//   virtual bool GetDecodedFrame(char** planes, int* strides);
//   virtual bool IsInitialized();
//   virtual int GetWidth();
//   virtual int GetHeight();
//   virtual int GetFormat();

//  private:
//   on2_codec_ctx_t codec_;
//   on2_codec_iter_t iter_;
//   bool first_frame_;
// };
