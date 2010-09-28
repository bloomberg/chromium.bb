// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/utils.h"

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "third_party/libjingle/source/talk/base/socket.h"

namespace remoting {

// TODO(sergeyu): This is a clone of MapPosixError() from
// net/socket/tcp_client_socket_libevent.cc. Move MapPosixError() to
// net/base/net_errors.cc and use it here.

// Convert values from <errno.h> to values from "net/base/net_errors.h"
int MapPosixToChromeError(int err) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (err) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return net::ERR_IO_PENDING;
    case ENETDOWN:
      return net::ERR_INTERNET_DISCONNECTED;
    case ETIMEDOUT:
      return net::ERR_TIMED_OUT;
    case ENOTCONN:
      return net::ERR_CONNECTION_CLOSED;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive
      return net::ERR_CONNECTION_RESET;
    case ECONNABORTED:
      return net::ERR_CONNECTION_ABORTED;
    case ECONNREFUSED:
      return net::ERR_CONNECTION_REFUSED;
    case EHOSTUNREACH:
    case ENETUNREACH:
      return net::ERR_ADDRESS_UNREACHABLE;
    case EADDRNOTAVAIL:
      return net::ERR_ADDRESS_INVALID;
    case 0:
      return net::OK;
    default:
      LOG(WARNING) << "Unknown error " << err << " mapped to net::ERR_FAILED";
      return net::ERR_FAILED;
  }
}

}  // namespace remoting
