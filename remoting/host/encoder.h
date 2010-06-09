// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_ENCODER_H_
#define REMOTING_HOST_ENCODER_H_

#include "base/basictypes.h"
#include "base/task.h"
#include "remoting/base/protocol/chromotocol.pb.h"
#include "remoting/host/capturer.h"

namespace media {

class DataBuffer;

}  // namespace media

namespace remoting {

// A class to perform the task of encoding a continous stream of
// images.
// This class operates asynchronously to enable maximum throughput.
class Encoder {
 public:
  virtual ~Encoder() {}

  // Encode an image stored in |input_data|. |dirty_rects| contains
  // regions of update since last encode.
  //
  // If |key_frame| is true, the encoder should not reference
  // previous encode and encode the full frame.
  //
  // When encoded data is available, partial or full |data_available_task|
  // is called, data can be read from |data| and size is |data_size|.
  // After the last data available event and encode has completed,
  // |encode_done| is set to true and |data_available_task| is deleted.
  //
  // Note that |input_data| and |stride| are arrays of 3 elements.
  //
  // Implementation has to ensure that when |data_available_task| is called
  // output parameters are stable.
  virtual void Encode(const DirtyRects& dirty_rects,
                      const uint8** input_data,
                      const int* strides,
                      bool key_frame,
                      UpdateStreamPacketHeader* header,
                      scoped_refptr<media::DataBuffer>* output_data,
                      bool* encode_done,
                      Task* data_available_task) = 0;

  // Set the dimension of the incoming images. Need to call this before
  // calling Encode().
  virtual void SetSize(int width, int height) = 0;

  // Set the pixel format of the incoming images. Need to call this before
  // calling Encode().
  virtual void SetPixelFormat(PixelFormat pixel_format) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_ENCODER_H_
