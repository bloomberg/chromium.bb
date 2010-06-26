// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_PROTOCOL_UTIL_H_
#define REMOTING_BASE_PROTOCOL_UTIL_H_

#include "google/protobuf/message_lite.h"
#include "media/base/data_buffer.h"
#include "remoting/base/protocol/chromotocol.pb.h"

// This file defines utility methods used for encoding and decoding the protocol
// used in Chromoting.
namespace remoting {

// Serialize the Protocol Buffer message and provide sufficient framing for
// sending it over the wire.
// This will provide sufficient prefix and suffix for the receiver side to
// decode the message.
scoped_refptr<media::DataBuffer> SerializeAndFrameMessage(
    const google::protobuf::MessageLite& msg);

int GetBytesPerPixel(PixelFormat format);

}  // namespace remoting

#endif  // REMOTING_BASE_PROTOCOL_UTIL_H_
