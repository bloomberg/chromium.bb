// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "cc/paint/decoded_draw_image.h"

namespace cc {

DecodedDrawImage::DecodedDrawImage(sk_sp<const SkImage> image,
                                   sk_sp<SkColorFilter> dark_mode_color_filter,
                                   const SkSize& src_rect_offset,
                                   const SkSize& scale_adjustment,
                                   SkFilterQuality filter_quality)
    : image_(std::move(image)),
      dark_mode_color_filter_(std::move(dark_mode_color_filter)),
      src_rect_offset_(src_rect_offset),
      scale_adjustment_(scale_adjustment),
      filter_quality_(filter_quality) {}

DecodedDrawImage::DecodedDrawImage(const gpu::Mailbox& mailbox,
                                   SkFilterQuality filter_quality)
    : mailbox_(mailbox),
      src_rect_offset_(SkSize::MakeEmpty()),
      scale_adjustment_(SkSize::Make(1.f, 1.f)),
      filter_quality_(filter_quality) {}

DecodedDrawImage::DecodedDrawImage(
    base::Optional<uint32_t> transfer_cache_entry_id,
    sk_sp<SkColorFilter> dark_mode_color_filter,
    const SkSize& src_rect_offset,
    const SkSize& scale_adjustment,
    SkFilterQuality filter_quality,
    bool needs_mips)
    : transfer_cache_entry_id_(transfer_cache_entry_id),
      dark_mode_color_filter_(std::move(dark_mode_color_filter)),
      src_rect_offset_(src_rect_offset),
      scale_adjustment_(scale_adjustment),
      filter_quality_(filter_quality),
      transfer_cache_entry_needs_mips_(needs_mips) {}

DecodedDrawImage::DecodedDrawImage()
    : DecodedDrawImage(nullptr,
                       nullptr,
                       SkSize::MakeEmpty(),
                       SkSize::Make(1.f, 1.f),
                       kNone_SkFilterQuality) {}

DecodedDrawImage::DecodedDrawImage(const DecodedDrawImage&) = default;
DecodedDrawImage::DecodedDrawImage(DecodedDrawImage&&) = default;
DecodedDrawImage& DecodedDrawImage::operator=(const DecodedDrawImage&) =
    default;
DecodedDrawImage& DecodedDrawImage::operator=(DecodedDrawImage&&) = default;

DecodedDrawImage::~DecodedDrawImage() = default;

}  // namespace cc
