// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protocol_util.h"

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "talk/base/byteorder.h"

namespace remoting {

scoped_refptr<media::DataBuffer> SerializeAndFrameMessage(
    const google::protobuf::MessageLite& msg) {

  // Create a buffer with 4 extra bytes. This is used as prefix to write an
  // int32 of the serialized message size for framing.
  const int kExtraBytes = sizeof(int32);
  int size = msg.ByteSize() + kExtraBytes;
  scoped_refptr<media::DataBuffer> buffer = new media::DataBuffer(size);
  talk_base::SetBE32(buffer->GetWritableData(), msg.GetCachedSize());
  msg.SerializeWithCachedSizesToArray(buffer->GetWritableData() + kExtraBytes);
  buffer->SetDataSize(size);
  return buffer;
}

int GetBytesPerPixel(PixelFormat format) {
  // Note: The order is important here for performance. This is sorted from the
  // most common to the less common (PixelFormatAscii is mostly used
  // just for testing).
  switch (format) {
    case PixelFormatRgb24:  return 3;
    case PixelFormatRgb565: return 2;
    case PixelFormatRgb32:  return 4;
    case PixelFormatAscii:  return 1;
    default:
      NOTREACHED() << "Pixel format not supported";
      return 0;
  }
}

}  // namespace remoting
