// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ERROR_H_
#define REMOTING_PROTOCOL_ERROR_H_

namespace remoting {
namespace protocol {

// The UI implementations maintain corresponding definitions of this
// enumeration in webapp/error.js and
// android/java/src/org/chromium/chromoting/jni/JniInterface.java.
// Be sure to update these locations if you make any changes to the ordering.
enum ErrorCode {
  OK = 0,
  PEER_IS_OFFLINE,
  SESSION_REJECTED,
  INCOMPATIBLE_PROTOCOL,
  AUTHENTICATION_FAILED,
  CHANNEL_CONNECTION_ERROR,
  SIGNALING_ERROR,
  SIGNALING_TIMEOUT,
  HOST_OVERLOAD,
  UNKNOWN_ERROR,
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ERROR_H_
