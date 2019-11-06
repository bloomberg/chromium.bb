// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/image_transfer_cache_entry.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace cc {
namespace {

// TODO(ericrk): Replace calls to this with calls to SkImage::makeTextureImage,
// once that function handles colorspaces. https://crbug.com/834837
sk_sp<SkImage> MakeTextureImage(GrContext* context,
                                sk_sp<SkImage> source_image,
                                sk_sp<SkColorSpace> target_color_space,
                                GrMipMapped mip_mapped) {
  // Step 1: Upload image and generate mips if necessary. If we will be applying
  // a color-space conversion, don't generate mips yet, instead do it after
  // conversion, in step 3.
  // NOTE: |target_color_space| is only passed over the transfer cache if needed
  // (non-null, different from the source color space).
  bool add_mips_after_color_conversion =
      target_color_space && mip_mapped == GrMipMapped::kYes;
  sk_sp<SkImage> uploaded_image = source_image->makeTextureImage(
      context, nullptr,
      add_mips_after_color_conversion ? GrMipMapped::kNo : mip_mapped);

  // Step 2: Apply a color-space conversion if necessary.
  if (uploaded_image && target_color_space) {
    // TODO(ericrk): consider adding in the DeleteSkImageAndPreventCaching
    // optimization from GpuImageDecodeCache where we forcefully remove the
    // intermediate from Skia's cache.
    uploaded_image = uploaded_image->makeColorSpace(target_color_space);
  }

  // Step 3: If we had a colorspace conversion, we couldn't mipmap in step 1, so
  // add mips here.
  if (uploaded_image && add_mips_after_color_conversion) {
    // TODO(ericrk): consider adding in the DeleteSkImageAndPreventCaching
    // optimization from GpuImageDecodeCache where we forcefully remove the
    // intermediate from Skia's cache.
    uploaded_image =
        uploaded_image->makeTextureImage(context, nullptr, GrMipMapped::kYes);
  }

  return uploaded_image;
}

}  // namespace

ClientImageTransferCacheEntry::ClientImageTransferCacheEntry(
    const SkPixmap* pixmap,
    const SkColorSpace* target_color_space,
    bool needs_mips)
    : id_(s_next_id_.GetNext()),
      pixmap_(pixmap),
      target_color_space_(target_color_space),
      needs_mips_(needs_mips) {
  size_t target_color_space_size =
      target_color_space ? target_color_space->writeToMemory(nullptr) : 0u;
  size_t pixmap_color_space_size =
      pixmap_->colorSpace() ? pixmap_->colorSpace()->writeToMemory(nullptr)
                            : 0u;

  // Compute and cache the size of the data.
  base::CheckedNumeric<uint32_t> safe_size;
  safe_size += PaintOpWriter::HeaderBytes();
  safe_size += sizeof(uint32_t);  // color type
  safe_size += sizeof(uint32_t);  // width
  safe_size += sizeof(uint32_t);  // height
  safe_size += sizeof(uint32_t);  // has mips
  safe_size += sizeof(uint64_t) + alignof(uint64_t);  // pixels size
  safe_size += target_color_space_size + sizeof(uint64_t) + alignof(uint64_t);
  safe_size += pixmap_color_space_size + sizeof(uint64_t) + alignof(uint64_t);
  // Include 4 bytes of padding so we can always align our data pointer to a
  // 4-byte boundary.
  safe_size += 4;
  safe_size += pixmap_->computeByteSize();
  size_ = safe_size.ValueOrDie();
}

ClientImageTransferCacheEntry::~ClientImageTransferCacheEntry() = default;

// static
base::AtomicSequenceNumber ClientImageTransferCacheEntry::s_next_id_;

uint32_t ClientImageTransferCacheEntry::SerializedSize() const {
  return size_;
}

uint32_t ClientImageTransferCacheEntry::Id() const {
  return id_;
}

bool ClientImageTransferCacheEntry::Serialize(base::span<uint8_t> data) const {
  DCHECK_GE(data.size(), SerializedSize());
  DCHECK_GT(pixmap_->width(), 0);
  DCHECK_GT(pixmap_->height(), 0);

  // We don't need to populate the SerializeOptions here since the writer is
  // only used for serializing primitives.
  PaintOp::SerializeOptions options(nullptr, nullptr, nullptr, nullptr, nullptr,
                                    nullptr, false, false, 0, 0, SkMatrix::I());
  PaintOpWriter writer(data.data(), data.size(), options);
  writer.Write(pixmap_->colorType());
  writer.Write(pixmap_->width());
  writer.Write(pixmap_->height());
  writer.Write(static_cast<uint32_t>(needs_mips_ ? 1 : 0));
  size_t pixmap_size = pixmap_->computeByteSize();
  writer.WriteSize(pixmap_size);
  // TODO(enne): we should consider caching these in some form.
  writer.Write(pixmap_->colorSpace());
  writer.Write(target_color_space_);
  writer.AlignMemory(4);
  writer.WriteData(pixmap_size, pixmap_->addr());

  // Size can't be 0 after serialization unless the writer has become invalid.
  if (writer.size() == 0u)
    return false;

  return true;
}

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry() = default;
ServiceImageTransferCacheEntry::~ServiceImageTransferCacheEntry() = default;

ServiceImageTransferCacheEntry::ServiceImageTransferCacheEntry(
    ServiceImageTransferCacheEntry&& other) = default;
ServiceImageTransferCacheEntry& ServiceImageTransferCacheEntry::operator=(
    ServiceImageTransferCacheEntry&& other) = default;

bool ServiceImageTransferCacheEntry::BuildFromHardwareDecodedImage(
    GrContext* context,
    std::vector<sk_sp<SkImage>> plane_images,
    size_t buffer_byte_size,
    bool needs_mips,
    sk_sp<SkColorSpace> target_color_space) {
  context_ = context;

  // 1) Extract the planar textures from |plane_images|.
  std::vector<GrBackendTexture> plane_backend_textures(3u);
  DCHECK_EQ(3u, plane_images.size());
  for (size_t plane = 0; plane < 3u; plane++) {
    plane_backend_textures[plane] = plane_images[plane]->getBackendTexture(
        true /* flushPendingGrContextIO */);
    if (!plane_backend_textures[plane].isValid()) {
      DLOG(ERROR) << "Invalid backend texture found";
      return false;
    }
    if (needs_mips) {
      // TODO(andrescj): generate mipmaps when requested. This will require some
      // resource management: we either let Skia own the new textures or we take
      // ownership and delete them in |destroy_callback|.
      NOTIMPLEMENTED();
    }
  }
  plane_images_ = std::move(plane_images);

  // 2) Create a SkImage backed by the YUV textures extracted above. There are
  //    two assumptions here:
  //
  //    - SkYUVColorSpace::kJPEG_SkYUVColorSpace is used for the YUV-to-RGB
  //      matrix.
  //    - The color space of the resulting image is sRGB.
  //
  // TODO(andrescj): support other YUV-to-RGB conversions and embedded color
  // profiles.
  SkYUVAIndex plane_indices[] = {
      SkYUVAIndex{0, SkColorChannel::kR}, SkYUVAIndex{1, SkColorChannel::kR},
      SkYUVAIndex{2, SkColorChannel::kR}, SkYUVAIndex{-1, SkColorChannel::kR}};
  image_ = SkImage::MakeFromYUVATextures(
      context_, SkYUVColorSpace::kJPEG_SkYUVColorSpace,
      plane_backend_textures.data(), plane_indices,
      plane_images_[0]->dimensions(), kTopLeft_GrSurfaceOrigin);
  if (!image_) {
    DLOG(ERROR) << "Could not create YUV SkImage";
    return false;
  }

  // 3) Perform color space conversion if necessary.
  if (target_color_space)
    image_ = image_->makeColorSpace(target_color_space);
  if (!image_) {
    DLOG(ERROR) << "Could not do color space conversion";
    return false;
  }

  // 4) Fill out the rest of the information.
  has_mips_ = false;
  size_ = buffer_byte_size;
  fits_on_gpu_ = true;
  return true;
}

size_t ServiceImageTransferCacheEntry::CachedSize() const {
  return size_;
}

bool ServiceImageTransferCacheEntry::Deserialize(
    GrContext* context,
    base::span<const uint8_t> data) {
  context_ = context;

  // We don't need to populate the DeSerializeOptions here since the reader is
  // only used for de-serializing primitives.
  std::vector<uint8_t> scratch_buffer;
  PaintOp::DeserializeOptions options(nullptr, nullptr, nullptr,
                                      &scratch_buffer);
  PaintOpReader reader(data.data(), data.size(), options);
  SkColorType color_type = kUnknown_SkColorType;
  reader.Read(&color_type);

  if (color_type == kUnknown_SkColorType ||
      color_type == kRGB_101010x_SkColorType ||
      color_type > kLastEnum_SkColorType)
    return false;

  uint32_t width;
  reader.Read(&width);
  uint32_t height;
  reader.Read(&height);
  uint32_t needs_mips;
  reader.Read(&needs_mips);
  has_mips_ = needs_mips;
  size_t pixel_size;
  reader.ReadSize(&pixel_size);
  sk_sp<SkColorSpace> pixmap_color_space;
  reader.Read(&pixmap_color_space);
  sk_sp<SkColorSpace> target_color_space;
  reader.Read(&target_color_space);

  if (!reader.valid())
    return false;

  SkImageInfo image_info = SkImageInfo::Make(
      width, height, color_type, kPremul_SkAlphaType, pixmap_color_space);
  if (image_info.computeMinByteSize() > pixel_size)
    return false;

  // Align data to a 4-byte boundry, to match what we did when writing.
  reader.AlignMemory(4);
  const volatile void* pixel_data = reader.ExtractReadableMemory(pixel_size);
  if (!reader.valid())
    return false;

  DCHECK(SkIsAlign4(reinterpret_cast<uintptr_t>(pixel_data)));

  if (width == 0 || height == 0)
    return false;

  // Match GrTexture::onGpuMemorySize so that memory traces agree.
  auto gr_mips = has_mips_ ? GrMipMapped::kYes : GrMipMapped::kNo;
  size_ = GrContext::ComputeTextureSize(color_type, width, height, gr_mips);

  // Const-cast away the "volatile" on |pixel_data|. We specifically understand
  // that a malicious caller may change our pixels under us, and are OK with
  // this as the worst case scenario is visual corruption.
  SkPixmap pixmap(image_info, const_cast<const void*>(pixel_data),
                  image_info.minRowBytes());
  return MakeSkImage(pixmap, width, height, target_color_space);
}

bool ServiceImageTransferCacheEntry::MakeSkImage(
    const SkPixmap& pixmap,
    uint32_t width,
    uint32_t height,
    sk_sp<SkColorSpace> target_color_space) {
  DCHECK(context_);

  // Depending on whether the pixmap will fit in a GPU texture, either create
  // a software or GPU SkImage.
  uint32_t max_size = context_->maxTextureSize();
  fits_on_gpu_ = width <= max_size && height <= max_size;
  if (fits_on_gpu_) {
    sk_sp<SkImage> image = SkImage::MakeFromRaster(pixmap, nullptr, nullptr);
    if (!image)
      return false;
    image_ = MakeTextureImage(context_, std::move(image), target_color_space,
                              has_mips_ ? GrMipMapped::kYes : GrMipMapped::kNo);
  } else {
    sk_sp<SkImage> original =
        SkImage::MakeFromRaster(pixmap, [](const void*, void*) {}, nullptr);
    if (!original)
      return false;
    if (target_color_space) {
      image_ = original->makeColorSpace(target_color_space);
      // If color space conversion is a noop, use original data.
      if (image_ == original)
        image_ = SkImage::MakeRasterCopy(pixmap);
    } else {
      // No color conversion to do, use original data.
      image_ = SkImage::MakeRasterCopy(pixmap);
    }
  }

  return !!image_;
}

void ServiceImageTransferCacheEntry::EnsureMips() {
  if (has_mips_)
    return;

  if (!plane_images_.empty()) {
    // TODO(andrescj): generate mipmaps for hardware-accelerated decodes when
    // requested. This will require some resource management: we either let Skia
    // own the new textures or we take ownership and delete them in
    // |destroy_callback_|.
    NOTIMPLEMENTED();
  }

  has_mips_ = true;
  // TODO(ericrk): consider adding in the DeleteSkImageAndPreventCaching
  // optimization from GpuImageDecodeCache where we forcefully remove the
  // intermediate from Skia's cache.
  image_ = image_->makeTextureImage(context_, nullptr, GrMipMapped::kYes);
}

}  // namespace cc
