// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_FRAMES_QUIC_CONNECTION_CLOSE_FRAME_H_
#define QUICHE_QUIC_CORE_FRAMES_QUIC_CONNECTION_CLOSE_FRAME_H_

#include <ostream>
#include <string>

#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"

namespace quic {

// There are three different forms of CONNECTION_CLOSE.
typedef enum QuicConnectionCloseType {
  GOOGLE_QUIC_CONNECTION_CLOSE = 0,
  IETF_QUIC_TRANSPORT_CONNECTION_CLOSE = 1,
  IETF_QUIC_APPLICATION_CONNECTION_CLOSE = 2
} QuicConnectionCloseType;
QUIC_EXPORT_PRIVATE std::ostream& operator<<(
    std::ostream& os,
    const QuicConnectionCloseType type);

struct QUIC_EXPORT_PRIVATE QuicConnectionCloseFrame {
  QuicConnectionCloseFrame();
  QuicConnectionCloseFrame(QuicErrorCode error_code);
  QuicConnectionCloseFrame(QuicErrorCode error_code, std::string error_details);
  QuicConnectionCloseFrame(QuicIetfTransportErrorCodes transport_error_code,
                           QuicErrorCode extracted_error_code,
                           std::string error_details,
                           uint64_t frame_type);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicConnectionCloseFrame& c);

  // Indicates whether the received CONNECTION_CLOSE frame is a Google QUIC
  // CONNECTION_CLOSE, IETF QUIC CONNECTION_CLOSE.
  QuicConnectionCloseType close_type;

  // This is the error field in the frame.
  // The CONNECTION_CLOSE frame reports a 16-bit error code:
  // - The transport error code as reported in a CONNECTION_CLOSE/Transport
  //   frame,
  // - An opaque 16-bit code as reported in CONNECTION_CLOSE/Application frames,
  // - A QuicErrorCode, which is used in Google QUIC.
  union {
    QuicIetfTransportErrorCodes transport_error_code;
    uint16_t application_error_code;
    QuicErrorCode quic_error_code;
  };

  // This error code is extracted from, or added to, the "QuicErrorCode:
  // QUIC_...(123)" text in the error_details. It provides fine-grained
  // information as to the source of the error.
  QuicErrorCode extracted_error_code;

  // String with additional error details. "QuicErrorCode: 123" will be appended
  // to the error details when sending IETF QUIC Connection Close and
  // Application Close frames and parsed into extracted_error_code upon receipt,
  // when present.
  std::string error_details;

  // The frame type present in the IETF transport connection close frame.
  // Not populated for the Google QUIC or application connection close frames.
  // Contains the type of frame that triggered the connection close. Made a
  // uint64, as opposed to the QuicIetfFrameType, to support possible
  // extensions as well as reporting invalid frame types received from the peer.
  uint64_t transport_close_frame_type;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_FRAMES_QUIC_CONNECTION_CLOSE_FRAME_H_
