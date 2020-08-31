// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_FRAMES_QUIC_RETIRE_CONNECTION_ID_FRAME_H_
#define QUICHE_QUIC_CORE_FRAMES_QUIC_RETIRE_CONNECTION_ID_FRAME_H_

#include <ostream>

#include "net/third_party/quiche/src/quic/core/quic_constants.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_uint128.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicRetireConnectionIdFrame {
  QuicRetireConnectionIdFrame() = default;
  QuicRetireConnectionIdFrame(QuicControlFrameId control_frame_id,
                              QuicConnectionIdSequenceNumber sequence_number);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicRetireConnectionIdFrame& frame);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id = kInvalidControlFrameId;
  QuicConnectionIdSequenceNumber sequence_number = 0;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_FRAMES_QUIC_RETIRE_CONNECTION_ID_FRAME_H_
