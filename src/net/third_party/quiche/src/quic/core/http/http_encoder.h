// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_HTTP_HTTP_ENCODER_H_
#define QUICHE_QUIC_CORE_HTTP_HTTP_ENCODER_H_

#include <cstddef>

#include "net/third_party/quiche/src/quic/core/http/http_frames.h"
#include "net/third_party/quiche/src/quic/core/quic_error_codes.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

class QuicDataWriter;

// A class for encoding the HTTP frames that are exchanged in an HTTP over QUIC
// session.
class QUIC_EXPORT_PRIVATE HttpEncoder {
 public:
  HttpEncoder();

  ~HttpEncoder();

  // Serializes a DATA frame header into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeDataFrameHeader(QuicByteCount payload_length,
                                         std::unique_ptr<char[]>* output);

  // Serializes a HEADERS frame header into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeHeadersFrameHeader(QuicByteCount payload_length,
                                            std::unique_ptr<char[]>* output);

  // Serializes a PRIORITY frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializePriorityFrame(const PriorityFrame& priority,
                                       std::unique_ptr<char[]>* output);

  // Serializes a CANCEL_PUSH frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeCancelPushFrame(const CancelPushFrame& cancel_push,
                                         std::unique_ptr<char[]>* output);

  // Serializes a SETTINGS frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeSettingsFrame(const SettingsFrame& settings,
                                       std::unique_ptr<char[]>* output);

  // Serializes the header and push_id of a PUSH_PROMISE frame into a new buffer
  // stored in |output|. Returns the length of the buffer on success, or 0
  // otherwise.
  QuicByteCount SerializePushPromiseFrameWithOnlyPushId(
      const PushPromiseFrame& push_promise,
      std::unique_ptr<char[]>* output);

  // Serializes a GOAWAY frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeGoAwayFrame(const GoAwayFrame& goaway,
                                     std::unique_ptr<char[]>* output);

  // Serializes a MAX_PUSH frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeMaxPushIdFrame(const MaxPushIdFrame& max_push_id,
                                        std::unique_ptr<char[]>* output);

  // Serialize a DUPLICATE_PUSH frame into a new buffer stored in |output|.
  // Returns the length of the buffer on success, or 0 otherwise.
  QuicByteCount SerializeDuplicatePushFrame(
      const DuplicatePushFrame& duplicate_push,
      std::unique_ptr<char[]>* output);

 private:
  bool WriteFrameHeader(QuicByteCount length,
                        HttpFrameType type,
                        QuicDataWriter* writer);

  QuicByteCount GetTotalLength(QuicByteCount payload_length,
                               HttpFrameType type);
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_HTTP_HTTP_ENCODER_H_
