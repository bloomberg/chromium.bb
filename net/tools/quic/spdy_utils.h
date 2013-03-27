// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_QUIC_SPDY_UTILS_H_
#define NET_TOOLS_QUIC_SPDY_UTILS_H_

#include <string>

#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_protocol.h"
#include "net/tools/flip_server/balsa_headers.h"

namespace net {

class SpdyUtils {
 public:
  static std::string SerializeRequestHeaders(
      const BalsaHeaders& request_headers);

  static std::string SerializeResponseHeaders(
      const BalsaHeaders& response_headers);

  static bool FillBalsaRequestHeaders(const SpdyHeaderBlock& header_block,
                                      BalsaHeaders* request_headers);

  static bool FillBalsaResponseHeaders(const SpdyHeaderBlock& header_block,
                                       BalsaHeaders* response_headers);
};

}  // namespace net

#endif  // NET_TOOLS_QUIC_SPDY_UTILS_H_
