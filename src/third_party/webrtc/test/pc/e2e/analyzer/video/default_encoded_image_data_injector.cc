/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_encoded_image_data_injector.h"

#include <algorithm>
#include <cstddef>

#include "absl/memory/memory.h"
#include "api/video/encoded_image.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {

// The amount on which encoded image buffer will be expanded to inject frame id.
// This is 2 bytes for uint16_t frame id itself and 4 bytes for original length
// of the buffer.
constexpr int kEncodedImageBufferExpansion = 6;
constexpr size_t kInitialBufferSize = 2 * 1024;
// Count of coding entities for which buffers pools will be added on
// construction.
constexpr int kPreInitCodingEntitiesCount = 2;
constexpr size_t kBuffersPoolPerCodingEntity = 256;

struct ExtractionInfo {
  size_t length;
  bool discard;
};

}  // namespace

DefaultEncodedImageDataInjector::DefaultEncodedImageDataInjector() {
  for (size_t i = 0;
       i < kPreInitCodingEntitiesCount * kBuffersPoolPerCodingEntity; ++i) {
    bufs_pool_.push_back(
        absl::make_unique<std::vector<uint8_t>>(kInitialBufferSize));
  }
}
DefaultEncodedImageDataInjector::~DefaultEncodedImageDataInjector() = default;

EncodedImage DefaultEncodedImageDataInjector::InjectData(
    uint16_t id,
    bool discard,
    const EncodedImage& source,
    int coding_entity_id) {
  ExtendIfRequired(coding_entity_id);

  EncodedImage out = source;
  out.Retain();
  std::vector<uint8_t>* buffer = NextBuffer();
  if (buffer->size() < source.size() + kEncodedImageBufferExpansion) {
    buffer->resize(source.size() + kEncodedImageBufferExpansion);
  }
  out.set_buffer(buffer->data(), buffer->size());
  out.set_size(source.size() + kEncodedImageBufferExpansion);
  memcpy(out.data(), source.data(), source.size());
  size_t insertion_pos = source.size();
  out.data()[insertion_pos] = id & 0x00ff;
  out.data()[insertion_pos + 1] = (id & 0xff00) >> 8;
  out.data()[insertion_pos + 2] = source.size() & 0x000000ff;
  out.data()[insertion_pos + 3] = (source.size() & 0x0000ff00) >> 8;
  out.data()[insertion_pos + 4] = (source.size() & 0x00ff0000) >> 16;
  out.data()[insertion_pos + 5] = (source.size() & 0xff000000) >> 24;

  // We will store discard flag in the high bit of high byte of the size.
  RTC_CHECK_LT(source.size(), 1U << 31) << "High bit is already in use";
  out.data()[insertion_pos + 5] =
      out.data()[insertion_pos + 5] | ((discard ? 1 : 0) << 7);
  return out;
}

EncodedImageExtractionResult DefaultEncodedImageDataInjector::ExtractData(
    const EncodedImage& source,
    int coding_entity_id) {
  ExtendIfRequired(coding_entity_id);

  EncodedImage out = source;
  std::vector<uint8_t>* buffer = NextBuffer();
  if (buffer->size() < source.capacity() - kEncodedImageBufferExpansion) {
    buffer->resize(source.capacity() - kEncodedImageBufferExpansion);
  }
  out.set_buffer(buffer->data(), buffer->size());

  size_t source_pos = source.size() - 1;
  absl::optional<uint16_t> id = absl::nullopt;
  bool discard = true;
  std::vector<ExtractionInfo> extraction_infos;
  // First make a reverse pass through whole buffer to populate frame id,
  // discard flags and concatenated encoded images length.
  while (true) {
    size_t insertion_pos = source_pos - kEncodedImageBufferExpansion + 1;
    RTC_CHECK_GE(insertion_pos, 0);
    RTC_CHECK_LE(insertion_pos + kEncodedImageBufferExpansion, source.size());
    uint16_t next_id =
        source.data()[insertion_pos] + (source.data()[insertion_pos + 1] << 8);
    RTC_CHECK(!id || id.value() == next_id)
        << "Different frames encoded into single encoded image: " << id.value()
        << " vs " << next_id;
    id = next_id;
    uint32_t length = source.data()[insertion_pos + 2] +
                      (source.data()[insertion_pos + 3] << 8) +
                      (source.data()[insertion_pos + 4] << 16) +
                      ((source.data()[insertion_pos + 5] << 24) & 0b01111111);
    bool current_discard = (source.data()[insertion_pos + 5] & 0b10000000) != 0;
    extraction_infos.push_back({length, current_discard});
    // Extraction result is discarded only if all encoded partitions are
    // discarded.
    discard = discard && current_discard;
    if (source_pos < length + kEncodedImageBufferExpansion) {
      break;
    }
    source_pos -= length + kEncodedImageBufferExpansion;
  }
  RTC_CHECK(id);
  std::reverse(extraction_infos.begin(), extraction_infos.end());
  if (discard) {
    out.set_size(0);
    return EncodedImageExtractionResult{*id, out, true};
  }

  // Now basing on populated data make a forward pass to copy required pieces
  // of data to the output buffer.
  source_pos = 0;
  size_t out_pos = 0;
  auto extraction_infos_it = extraction_infos.begin();
  while (source_pos < source.size()) {
    const ExtractionInfo& info = *extraction_infos_it;
    RTC_CHECK_LE(source_pos + kEncodedImageBufferExpansion + info.length,
                 source.size());
    if (!info.discard) {
      // Copy next encoded image payload from concatenated buffer only if it is
      // not discarded.
      memcpy(&out.data()[out_pos], &source.data()[source_pos], info.length);
      out_pos += info.length;
    }
    source_pos += info.length + kEncodedImageBufferExpansion;
    ++extraction_infos_it;
  }
  out.set_size(out_pos);

  return EncodedImageExtractionResult{id.value(), out, discard};
}

void DefaultEncodedImageDataInjector::ExtendIfRequired(int coding_entity_id) {
  rtc::CritScope crit(&lock_);
  if (coding_entities_.find(coding_entity_id) != coding_entities_.end()) {
    // This entity is already known for this injector, so buffers are allocated.
    return;
  }

  // New coding entity. We need allocate extra buffers for this encoder/decoder
  // We will put them into front of the queue to use them first.
  coding_entities_.insert(coding_entity_id);
  if (coding_entities_.size() <= kPreInitCodingEntitiesCount) {
    // Buffers for the first kPreInitCodingEntitiesCount coding entities were
    // allocated during construction.
    return;
  }
  for (size_t i = 0; i < kBuffersPoolPerCodingEntity; ++i) {
    bufs_pool_.push_front(
        absl::make_unique<std::vector<uint8_t>>(kInitialBufferSize));
  }
}

std::vector<uint8_t>* DefaultEncodedImageDataInjector::NextBuffer() {
  rtc::CritScope crit(&lock_);
  // Get buffer from the front of the queue, return it to the caller and
  // put in the back
  std::vector<uint8_t>* out = bufs_pool_.front().get();
  bufs_pool_.push_back(std::move(bufs_pool_.front()));
  bufs_pool_.pop_front();
  return out;
}

}  // namespace test
}  // namespace webrtc
