// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_MEDIA_STREAM_FRAME_H_
#define PPAPI_SHARED_IMPL_MEDIA_STREAM_FRAME_H_

#include "ppapi/c/ppb_video_frame.h"

namespace ppapi {

union MediaStreamFrame {
  enum Type {
    TYPE_UNKNOWN = 0,
    TYPE_AUDIO = 1,
    TYPE_VIDEO = 2,
  };

  struct Header {
    Type type;
    uint32_t size;
  };

  struct Audio {
    Header header;
    // TODO(penghuang): implement the audio frame.
  };

  struct Video {
    Header header;
    PP_TimeDelta timestamp;
    PP_VideoFrame_Format format;
    PP_Size size;
    uint32_t data_size;
    // Uses 8 bytes to make sure the Video struct has consistent size between
    // Nacl code and renderer code.
    uint8_t data[8];
  };

  // Because these structs are written and read in shared memory, we need
  // the size and alighment to be consistent between NaCl and its host trusted
  // platform.
  PP_COMPILE_ASSERT_SIZE_IN_BYTES(Header, 8);
  PP_COMPILE_ASSERT_SIZE_IN_BYTES(Audio, 8);
  PP_COMPILE_ASSERT_SIZE_IN_BYTES(Video, 40);

  Header header;
  Video video;
  Audio audio;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_MEDIA_STREAM_FRAME_H_
