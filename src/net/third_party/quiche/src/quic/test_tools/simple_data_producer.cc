// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/test_tools/simple_data_producer.h"

#include <utility>

#include "absl/strings/string_view.h"
#include "quic/core/quic_data_writer.h"
#include "quic/platform/api/quic_bug_tracker.h"
#include "quic/platform/api/quic_flags.h"

namespace quic {

namespace test {

SimpleDataProducer::SimpleDataProducer() {}

SimpleDataProducer::~SimpleDataProducer() {}

void SimpleDataProducer::SaveStreamData(QuicStreamId id,
                                        const struct iovec* iov,
                                        int iov_count,
                                        size_t iov_offset,
                                        QuicByteCount data_length) {
  if (data_length == 0) {
    return;
  }
  if (!send_buffer_map_.contains(id)) {
    send_buffer_map_[id] = std::make_unique<QuicStreamSendBuffer>(&allocator_);
  }
  send_buffer_map_[id]->SaveStreamData(iov, iov_count, iov_offset, data_length);
}

void SimpleDataProducer::SaveCryptoData(EncryptionLevel level,
                                        QuicStreamOffset offset,
                                        absl::string_view data) {
  auto key = std::make_pair(level, offset);
  crypto_buffer_map_[key] = data;
}

WriteStreamDataResult SimpleDataProducer::WriteStreamData(
    QuicStreamId id,
    QuicStreamOffset offset,
    QuicByteCount data_length,
    QuicDataWriter* writer) {
  auto iter = send_buffer_map_.find(id);
  if (iter == send_buffer_map_.end()) {
    return STREAM_MISSING;
  }
  if (iter->second->WriteStreamData(offset, data_length, writer)) {
    return WRITE_SUCCESS;
  }
  return WRITE_FAILED;
}

bool SimpleDataProducer::WriteCryptoData(EncryptionLevel level,
                                         QuicStreamOffset offset,
                                         QuicByteCount data_length,
                                         QuicDataWriter* writer) {
  auto it = crypto_buffer_map_.find(std::make_pair(level, offset));
  if (it == crypto_buffer_map_.end() || it->second.length() < data_length) {
    return false;
  }
  return writer->WriteStringPiece(
      absl::string_view(it->second.data(), data_length));
}

}  // namespace test

}  // namespace quic
