// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/errors.h"

#include "base/logging.h"

namespace remoting {
namespace protocol {

#define RETURN_STRING_LITERAL(x) \
case x: \
return #x;

const char* ErrorCodeToString(ErrorCode error) {
  switch (error) {
    RETURN_STRING_LITERAL(OK);
    RETURN_STRING_LITERAL(PEER_IS_OFFLINE);
    RETURN_STRING_LITERAL(SESSION_REJECTED);
    RETURN_STRING_LITERAL(INCOMPATIBLE_PROTOCOL);
    RETURN_STRING_LITERAL(AUTHENTICATION_FAILED);
    RETURN_STRING_LITERAL(CHANNEL_CONNECTION_ERROR);
    RETURN_STRING_LITERAL(SIGNALING_ERROR);
    RETURN_STRING_LITERAL(SIGNALING_TIMEOUT);
    RETURN_STRING_LITERAL(HOST_OVERLOAD);
    RETURN_STRING_LITERAL(MAX_SESSION_LENGTH);
    RETURN_STRING_LITERAL(HOST_CONFIGURATION_ERROR);
    RETURN_STRING_LITERAL(UNKNOWN_ERROR);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace protocol
}  // namespace remoting
