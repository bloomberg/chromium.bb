// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_serialization.h"

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "third_party/webrtc/base/byteorder.h"

namespace remoting {
namespace protocol {

scoped_refptr<net::IOBufferWithSize> SerializeAndFrameMessage(
    const google::protobuf::MessageLite& msg) {
  // Create a buffer with 4 extra bytes. This is used as prefix to write an
  // int32 of the serialized message size for framing.
  const int kExtraBytes = sizeof(int32);
  int size = msg.ByteSize() + kExtraBytes;
  scoped_refptr<net::IOBufferWithSize> buffer(new net::IOBufferWithSize(size));
  rtc::SetBE32(buffer->data(), msg.GetCachedSize());
  msg.SerializeWithCachedSizesToArray(
      reinterpret_cast<uint8*>(buffer->data()) + kExtraBytes);
  return buffer;
}

}  // namespace protocol
}  // namespace remoting
