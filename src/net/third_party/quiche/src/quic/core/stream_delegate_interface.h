// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_STREAM_DELEGATE_INTERFACE_H_
#define QUICHE_QUIC_CORE_STREAM_DELEGATE_INTERFACE_H_

#include <cstddef>
#include "absl/types/optional.h"
#include "quic/core/quic_types.h"
#include "spdy/core/spdy_protocol.h"

namespace quic {

class QuicStream;

// Pure virtual class to get notified when particular QuicStream events
// occurred.
class QUIC_EXPORT_PRIVATE StreamDelegateInterface {
 public:
  virtual ~StreamDelegateInterface() {}

  // Called when the stream has encountered errors that it can't handle.
  virtual void OnStreamError(QuicErrorCode error_code,
                             std::string error_details) = 0;
  // Called when the stream has encountered errors that it can't handle,
  // specifying the wire error code |ietf_error| explicitly.
  virtual void OnStreamError(QuicErrorCode error_code,
                             QuicIetfTransportErrorCodes ietf_error,
                             std::string error_details) = 0;
  // Called when the stream needs to write data. If |level| is present, the data
  // will be written at the specified |level|. The data will be written
  // at specified transmission |type|.
  // TODO(fayang): Change absl::optional<EncryptionLevel> to EncryptionLevel
  // when deprecating quic_use_write_or_buffer_data_at_level.
  virtual QuicConsumedData WritevData(
      QuicStreamId id,
      size_t write_length,
      QuicStreamOffset offset,
      StreamSendingState state,
      TransmissionType type,
      absl::optional<EncryptionLevel> level) = 0;
  // Called to write crypto data.
  virtual size_t SendCryptoData(EncryptionLevel level,
                                size_t write_length,
                                QuicStreamOffset offset,
                                TransmissionType type) = 0;
  // Called on stream creation.
  virtual void RegisterStreamPriority(
      QuicStreamId id,
      bool is_static,
      const spdy::SpdyStreamPrecedence& precedence) = 0;
  // Called on stream destruction to clear priority.
  virtual void UnregisterStreamPriority(QuicStreamId id, bool is_static) = 0;
  // Called by the stream on SetPriority to update priority.
  virtual void UpdateStreamPriority(
      QuicStreamId id,
      const spdy::SpdyStreamPrecedence& new_precedence) = 0;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_STREAM_DELEGATE_INTERFACE_H_
