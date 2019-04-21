// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
#define CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/atomic_sequence_num.h"
#include "base/containers/span.h"
#include "cc/paint/transfer_cache_entry.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class GrContext;
class SkColorSpace;
class SkImage;
struct SkImageInfo;
class SkPixmap;

namespace cc {

static constexpr uint32_t kInvalidImageTransferCacheEntryId =
    static_cast<uint32_t>(-1);

// Client/ServiceImageTransferCacheEntry implement a transfer cache entry
// for transferring image data. On the client side, this is a CPU SkPixmap,
// on the service side the image is uploaded and is a GPU SkImage.
class CC_PAINT_EXPORT ClientImageTransferCacheEntry
    : public ClientTransferCacheEntryBase<TransferCacheEntryType::kImage> {
 public:
  explicit ClientImageTransferCacheEntry(const SkPixmap* pixmap,
                                         const SkColorSpace* target_color_space,
                                         bool needs_mips);
  ~ClientImageTransferCacheEntry() final;

  uint32_t Id() const final;

  // ClientTransferCacheEntry implementation:
  uint32_t SerializedSize() const final;
  bool Serialize(base::span<uint8_t> data) const final;

 private:
  uint32_t id_;
  const SkPixmap* const pixmap_;
  const SkColorSpace* const target_color_space_;
  const bool needs_mips_;
  uint32_t size_ = 0;
  static base::AtomicSequenceNumber s_next_id_;
};

class CC_PAINT_EXPORT ServiceImageTransferCacheEntry
    : public ServiceTransferCacheEntryBase<TransferCacheEntryType::kImage> {
 public:
  ServiceImageTransferCacheEntry();
  ~ServiceImageTransferCacheEntry() final;

  ServiceImageTransferCacheEntry(ServiceImageTransferCacheEntry&& other);
  ServiceImageTransferCacheEntry& operator=(
      ServiceImageTransferCacheEntry&& other);

  // Populates this entry using |decoded_image| described by |row_bytes| and
  // |image_info|. The image is uploaded to the GPU if its dimensions are both
  // at most |context_|->maxTextureSize().
  bool BuildFromDecodedData(GrContext* context,
                            base::span<const uint8_t> decoded_image,
                            size_t row_bytes,
                            const SkImageInfo& image_info,
                            bool needs_mips,
                            sk_sp<SkColorSpace> target_color_space);

  // ServiceTransferCacheEntry implementation:
  size_t CachedSize() const final;
  bool Deserialize(GrContext* context, base::span<const uint8_t> data) final;

  bool fits_on_gpu() const { return fits_on_gpu_; }
  const sk_sp<SkImage>& image() const { return image_; }

  // Ensures the cached image has mips.
  void EnsureMips();

 private:
  bool MakeSkImage(const SkPixmap& pixmap,
                   uint32_t width,
                   uint32_t height,
                   sk_sp<SkColorSpace> target_color_space);

  GrContext* context_ = nullptr;
  sk_sp<SkImage> image_;
  bool has_mips_ = false;
  size_t size_ = 0;
  bool fits_on_gpu_ = false;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_TRANSFER_CACHE_ENTRY_H_
