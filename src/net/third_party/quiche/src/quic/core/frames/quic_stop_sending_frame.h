// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_
#define QUICHE_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_

#include <ostream>

#include "net/third_party/quiche/src/quic/core/quic_constants.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicStopSendingFrame {
  QuicStopSendingFrame() = default;
  QuicStopSendingFrame(QuicControlFrameId control_frame_id,
                       QuicStreamId stream_id,
                       QuicApplicationErrorCode application_error_code);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicStopSendingFrame& frame);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id = kInvalidControlFrameId;
  QuicStreamId stream_id = 0;
  QuicApplicationErrorCode application_error_code = 0;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_FRAMES_QUIC_STOP_SENDING_FRAME_H_
