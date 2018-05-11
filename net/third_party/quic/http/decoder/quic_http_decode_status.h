// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_HTTP_DECODER_QUIC_HTTP_DECODE_STATUS_H_
#define NET_THIRD_PARTY_QUIC_HTTP_DECODER_QUIC_HTTP_DECODE_STATUS_H_

// Enum QuicHttpDecodeStatus is used to report the status of decoding of many
// types of HTTP/2 and HPQUIC_HTTP_ACK objects.

#include <ostream>

#include "net/third_party/quic/platform/api/quic_export.h"

namespace net {

enum class QuicHttpDecodeStatus {
  // Decoding is done.
  kDecodeDone,

  // Decoder needs more input to be able to make progress.
  kDecodeInProgress,

  // Decoding failed (e.g. HPQUIC_HTTP_ACK variable length integer is too large,
  // or
  // an HTTP/2 frame has padding declared to be larger than the payload).
  kDecodeError,
};
QUIC_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& out,
                                             QuicHttpDecodeStatus v);

}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_HTTP_DECODER_QUIC_HTTP_DECODE_STATUS_H_
