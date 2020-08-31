// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_cache.h"

#include <inttypes.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/containers/span.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/base/histograms.h"
#include "cc/raster/scoped_grcontext_access.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/mipmap_util.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/trace_util.h"

namespace cc {
namespace {
// The number or entries to keep in the cache, depending on the memory state of
// the system. This limit can be breached by in-use cache items, which cannot
// be deleted.
static const int kNormalMaxItemsInCacheForGpu = 2000;
static const int kSuspendedMaxItemsInCacheForGpu = 0;

// The maximum number of images that we can lock simultaneously in our working
// set. This is separate from the memory limit, as keeping very large numbers
// of small images simultaneously locked can lead to performance issues and
// memory spikes.
static const int kMaxItemsInWorkingSet = 256;

// lock_count │ used  │ result state
// ═══════════╪═══════╪══════════════════
//  1         │ false │ WASTED_ONCE
//  1         │ true  │ USED_ONCE
//  >1        │ false │ WASTED_RELOCKED
//  >1        │ true  │ USED_RELOCKED
// Note that it's important not to reorder the following enum, since the
// numerical values are used in the histogram code.
enum ImageUsageState : int {
  IMAGE_USAGE_STATE_WASTED_ONCE,
  IMAGE_USAGE_STATE_USED_ONCE,
  IMAGE_USAGE_STATE_WASTED_RELOCKED,
  IMAGE_USAGE_STATE_USED_RELOCKED,
  IMAGE_USAGE_STATE_COUNT
};

// Returns true if an image would not be drawn and should therefore be
// skipped rather than decoded.
bool SkipImage(const DrawImage& draw_image) {
  if (!SkIRect::Intersects(
          draw_image.src_rect(),
          SkIRect::MakeWH(draw_image.paint_image().width(),
                          draw_image.paint_image().height()))) {
    return true;
  }
  if (std::abs(draw_image.scale().width()) <
          std::numeric_limits<float>::epsilon() ||
      std::abs(draw_image.scale().height()) <
          std::numeric_limits<float>::epsilon()) {
    return true;
  }
  return false;
}

// Returns the filter quality to use for scaling the image to upload scale as
// well as for using when passing the decoded image to skia. Due to parity with
// SW and power impliciation, limit the filter quality to medium.
SkFilterQuality CalculateDesiredFilterQuality(const DrawImage& draw_image) {
  return std::min(kMedium_SkFilterQuality, draw_image.filter_quality());
}

// Calculate the mip level to upload-scale the image to before uploading. We use
// mip levels rather than exact scales to increase re-use of scaled images.
int CalculateUploadScaleMipLevel(const DrawImage& draw_image) {
  // Images which are being clipped will have color-bleeding if scaled.
  // TODO(ericrk): Investigate uploading clipped images to handle this case and
  // provide further optimization. crbug.com/620899
  if (draw_image.src_rect() !=
      SkIRect::MakeWH(draw_image.paint_image().width(),
                      draw_image.paint_image().height())) {
    return 0;
  }

  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  // Ceil our scaled size so that the mip map generated is guaranteed to be
  // larger. Take the abs of the scale, as mipmap functions don't handle
  // (and aren't impacted by) negative image dimensions.
  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(base_size, std::abs(draw_image.scale().width()),
                             std::abs(draw_image.scale().height()));

  return MipMapUtil::GetLevelForSize(base_size, scaled_size);
}

// Calculates the scale factor which can be used to scale an image to a given
// mip level.
SkSize CalculateScaleFactorForMipLevel(const DrawImage& draw_image,
                                       int upload_scale_mip_level) {
  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  return MipMapUtil::GetScaleAdjustmentForLevel(base_size,
                                                upload_scale_mip_level);
}

// Calculates the size of a given mip level.
gfx::Size CalculateSizeForMipLevel(const DrawImage& draw_image,
                                   int upload_scale_mip_level) {
  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  return MipMapUtil::GetSizeForLevel(base_size, upload_scale_mip_level);
}

// Determines whether a draw image requires mips.
bool ShouldGenerateMips(const DrawImage& draw_image,
                        int upload_scale_mip_level) {
  // If filter quality is less than medium, don't generate mips.
  if (draw_image.filter_quality() < kMedium_SkFilterQuality)
    return false;

  gfx::Size base_size(draw_image.paint_image().width(),
                      draw_image.paint_image().height());
  // Take the abs of the scale, as mipmap functions don't handle (and aren't
  // impacted by) negative image dimensions.
  gfx::SizeF scaled_size = gfx::ScaleSize(
      gfx::SizeF(base_size), std::abs(draw_image.scale().width()),
      std::abs(draw_image.scale().height()));

  // If our target size is smaller than our scaled size in both dimension, we
  // need to generate mips.
  gfx::SizeF target_size =
      gfx::SizeF(CalculateSizeForMipLevel(draw_image, upload_scale_mip_level));
  if (scaled_size.width() < target_size.width() &&
      scaled_size.height() < target_size.height()) {
    return true;
  }

  return false;
}

// This helper method takes in
//  - |yuva_size_info| corresponding to the plane dimensions of a YUV image
//  - |info| indicating SkAlphaType and SkColorSpace per plane (though the final
//    image color space is currently indicated through other means at creation
//    of the YUV SkImage)
//  - |memory_ptr| pointing to sufficient contiguous memory for the planes
//
// It then sets each SkPixmap to have the dimensions specified by its respective
// SkYUVAIndex within |yuva_size_info| and to point to bytes in memory at
// |planes[index]|.
void SetYuvPixmapsFromSizeInfo(SkPixmap* pixmap_y,
                               SkPixmap* pixmap_u,
                               SkPixmap* pixmap_v,
                               const SkYUVASizeInfo& yuva_size_info,
                               void* planes[SkYUVASizeInfo::kMaxCount],
                               const SkImageInfo& info,
                               void* memory_ptr) {
  DCHECK(pixmap_y);
  DCHECK(pixmap_u);
  DCHECK(pixmap_v);
  const size_t y_width_bytes =
      yuva_size_info.fWidthBytes[SkYUVAIndex::kY_Index];
  const size_t y_width = yuva_size_info.fSizes[SkYUVAIndex::kY_Index].width();
  const size_t y_height = yuva_size_info.fSizes[SkYUVAIndex::kY_Index].height();
  const size_t u_width_bytes =
      yuva_size_info.fWidthBytes[SkYUVAIndex::kU_Index];
  const size_t u_width = yuva_size_info.fSizes[SkYUVAIndex::kU_Index].width();
  const size_t u_height = yuva_size_info.fSizes[SkYUVAIndex::kU_Index].height();
  const size_t v_width_bytes =
      yuva_size_info.fWidthBytes[SkYUVAIndex::kV_Index];
  const size_t v_width = yuva_size_info.fSizes[SkYUVAIndex::kV_Index].width();
  const size_t v_height = yuva_size_info.fSizes[SkYUVAIndex::kV_Index].height();
  const SkImageInfo y_decode_info =
      info.makeColorType(kGray_8_SkColorType).makeWH(y_width, y_height);
  const SkImageInfo u_decode_info = y_decode_info.makeWH(u_width, u_height);
  const SkImageInfo v_decode_info = y_decode_info.makeWH(v_width, v_height);
  yuva_size_info.computePlanes(memory_ptr, planes);
  pixmap_y->reset(y_decode_info, planes[SkYUVAIndex::kY_Index], y_width_bytes);
  pixmap_u->reset(u_decode_info, planes[SkYUVAIndex::kU_Index], u_width_bytes);
  pixmap_v->reset(v_decode_info, planes[SkYUVAIndex::kV_Index], v_width_bytes);
}

// Helper method to fill in |scaled_u_size| and |scaled_v_size| by computing
// the mip level for each plane given the final raster dimensions and the
// unscaled U and V plane sizes. Also takes in |draw_image| to compute the Y
// plane mip level and DCHECK that the computed mip levels for U and V are
// reasonable.
//
// TODO(crbug.com/915972): Assumes 420 subsampling and DCHECKs the size ratios.
void ComputeMippedUVPlaneSizes(const gfx::Size& target_raster_size,
                               const gfx::Size& unscaled_u_size,
                               const gfx::Size& unscaled_v_size,
                               const DrawImage& draw_image,
                               gfx::Size* scaled_u_size,
                               gfx::Size* scaled_v_size) {
  DCHECK(scaled_u_size);
  DCHECK(scaled_v_size);
  DCHECK_EQ(unscaled_u_size, unscaled_v_size);
  DCHECK_EQ((draw_image.paint_image().width() + 1) / 2,
            unscaled_u_size.width());
  const int uv_mip_level =
      MipMapUtil::GetLevelForSize(unscaled_u_size, target_raster_size);
  // Check that the chroma planes do not shrink *more* than the luma.
  // At least for YUV420, they will shrink at most one mip level below luma,
  // which avoids blurriness.
  DCHECK_GE(uv_mip_level, 0);
  if (CalculateUploadScaleMipLevel(draw_image) == 0) {
    // If Y is not scaled, then U and V shouldn't be either.
    DCHECK_EQ(uv_mip_level, 0);
  } else {
    DCHECK_EQ(CalculateUploadScaleMipLevel(draw_image) - 1, uv_mip_level);
  }

  *scaled_u_size = MipMapUtil::GetSizeForLevel(unscaled_u_size, uv_mip_level);
  *scaled_v_size = *scaled_u_size;
}

// Estimates the byte size of the decoded data for an image that goes through
// hardware decode acceleration. The actual byte size is only known once the
// image is decoded in the service side because different drivers have different
// pixel format and alignment requirements.
size_t EstimateHardwareDecodedDataSize(
    const ImageHeaderMetadata* image_metadata) {
  gfx::Size dimensions = image_metadata->coded_size
                             ? *(image_metadata->coded_size)
                             : image_metadata->image_size;
  base::CheckedNumeric<size_t> y_data_size(dimensions.width());
  y_data_size *= dimensions.height();

  static_assert(
      // TODO(andrescj): refactor to instead have a static_assert at the
      // declaration site of gpu::ImageDecodeAcceleratorSubsampling to make sure
      // it has the same number of entries as YUVSubsampling.
      static_cast<int>(gpu::ImageDecodeAcceleratorSubsampling::kMaxValue) == 2,
      "EstimateHardwareDecodedDataSize() must be adapted to support all "
      "subsampling factors in ImageDecodeAcceleratorSubsampling");
  base::CheckedNumeric<size_t> uv_width(dimensions.width());
  base::CheckedNumeric<size_t> uv_height(dimensions.height());
  switch (image_metadata->yuv_subsampling) {
    case YUVSubsampling::k420:
      uv_width += 1u;
      uv_width /= 2u;
      uv_height += 1u;
      uv_height /= 2u;
      break;
    case YUVSubsampling::k422:
      uv_width += 1u;
      uv_width /= 2u;
      break;
    case YUVSubsampling::k444:
      break;
    default:
      NOTREACHED();
      return 0u;
  }
  base::CheckedNumeric<size_t> uv_data_size(uv_width * uv_height);
  return (y_data_size + 2 * uv_data_size).ValueOrDie();
}

// Draws and scales the provided |draw_image| into the |target_pixmap|. If the
// draw/scale can be done directly, calls directly into PaintImage::Decode.
// if not, decodes to a compatible temporary pixmap and then converts that into
// the |target_pixmap|.
//
// For RGBX decoding, the default, the parameters |pixmap_y|,
// |pixmap_u|, and |pixmap_v| are NULL. Otherwise, the pixmaps share a
// contiguous block of allocated backing memory. If scaling needs to happen,
// it is done individually for each plane.
//
// The |do_yuv_decode| parameter indicates whether YUV decoding can and should
// be done, which is a combination of the underlying data requesting YUV and the
// cache mode (i.e. OOP-R or not) supporting it.
bool DrawAndScaleImage(const DrawImage& draw_image,
                       SkPixmap* target_pixmap,
                       PaintImage::GeneratorClientId client_id,
                       const bool do_yuv_decode,
                       SkPixmap* pixmap_y = nullptr,
                       SkPixmap* pixmap_u = nullptr,
                       SkPixmap* pixmap_v = nullptr) {
  // We will pass color_space explicitly to PaintImage::Decode, so pull it out
  // of the pixmap and populate a stand-alone value.
  // Note: To pull colorspace out of the pixmap, we create a new pixmap with
  // null colorspace but the same memory pointer.
  // The backing memory for |pixmap| has been allocated based on
  // image_data->size, so it is correct for YUV even if the other parameters
  // for |pixmap| do not quite make sense for YUV (e.g. rowBytes).
  SkPixmap pixmap(target_pixmap->info().makeColorSpace(nullptr),
                  target_pixmap->writable_addr(), target_pixmap->rowBytes());
  uint8_t* data_ptr = reinterpret_cast<uint8_t*>(pixmap.writable_addr());
  sk_sp<SkColorSpace> color_space = target_pixmap->info().refColorSpace();

  const PaintImage& paint_image = draw_image.paint_image();
  const bool is_original_decode =
      SkISize::Make(paint_image.width(), paint_image.height()) ==
      pixmap.bounds().size();
  const bool is_nearest_neighbor =
      draw_image.filter_quality() == kNone_SkFilterQuality;
  SkImageInfo info = pixmap.info();
  SkYUVASizeInfo yuva_size_info;
  SkYUVAIndex plane_indices[SkYUVAIndex::kIndexCount];
  if (do_yuv_decode) {
    const bool yuva_info_initialized =
        paint_image.IsYuv(&yuva_size_info, plane_indices);
    DCHECK(yuva_info_initialized);
  }
  SkISize supported_size =
      paint_image.GetSupportedDecodeSize(pixmap.bounds().size());
  // We can directly decode into target pixmap if we are doing an original
  // decode or we are decoding to scale without nearest neighbor filtering.
  // TODO(crbug.com/927437): Although the JPEG decoder supports decoding to
  // scale, we have not yet implemented YUV + decoding to scale, so we skip it.
  const bool can_directly_decode =
      is_original_decode || (!is_nearest_neighbor && !do_yuv_decode);
  if (supported_size == pixmap.bounds().size() && can_directly_decode) {
    if (do_yuv_decode) {
      void* planes[SkYUVASizeInfo::kMaxCount];
      SetYuvPixmapsFromSizeInfo(pixmap_y, pixmap_u, pixmap_v, yuva_size_info,
                                planes, info, pixmap.writable_addr());
      return paint_image.DecodeYuv(planes, draw_image.frame_index(), client_id,
                                   yuva_size_info, plane_indices);
    }
    return paint_image.Decode(pixmap.writable_addr(), &info, color_space,
                              draw_image.frame_index(), client_id);
  }

  // If we can't decode/scale directly, we will handle this in 2 steps.
  // Step 1: Decode at the nearest (larger) directly supported size or the
  // original size if nearest neighbor quality is requested.
  // Step 2: Scale to |pixmap| size. If decoded image is half float backed and
  // the device does not support image resize, decode to N32 color type and
  // convert to F16 afterward. If doing YUV decoding, use an assumption of
  // YUV420 and the dimensions of |pixmap|. Resizing happens on a plane-by-plane
  // basis.
  SkImageInfo decode_info;
  if (do_yuv_decode) {
    const size_t yuva_bytes = yuva_size_info.computeTotalBytes();
    if (SkImageInfo::ByteSizeOverflowed(yuva_bytes)) {
      return false;
    }
    // We temporarily abuse the dimensions of the pixmap to ensure we allocate
    // the proper number of bytes, but the actual plane dimensions are stored in
    // |yuva_size_info| and accessed within PaintImage::DecodeYuv() and below.
    decode_info = info.makeColorType(kGray_8_SkColorType).makeWH(yuva_bytes, 1);
  } else {
    SkISize decode_size =
        is_nearest_neighbor
            ? SkISize::Make(paint_image.width(), paint_image.height())
            : supported_size;
    decode_info = info.makeWH(decode_size.width(), decode_size.height());
  }
  SkFilterQuality filter_quality = CalculateDesiredFilterQuality(draw_image);
  bool decode_to_f16_using_n32_intermediate =
      decode_info.colorType() == kRGBA_F16_SkColorType &&
      !ImageDecodeCacheUtils::CanResizeF16Image(filter_quality);
  if (decode_to_f16_using_n32_intermediate)
    decode_info = decode_info.makeColorType(kN32_SkColorType);

  SkBitmap decode_bitmap;
  if (!decode_bitmap.tryAllocPixels(decode_info))
    return false;

  SkPixmap decode_pixmap = decode_bitmap.pixmap();
  void* planes[SkYUVASizeInfo::kMaxCount];
  if (do_yuv_decode) {
    yuva_size_info.computePlanes(decode_pixmap.writable_addr(), planes);
  }
  bool initial_decode_failed =
      do_yuv_decode
          ? !paint_image.DecodeYuv(planes, draw_image.frame_index(), client_id,
                                   yuva_size_info, plane_indices)
          : !paint_image.Decode(decode_pixmap.writable_addr(), &decode_info,
                                color_space, draw_image.frame_index(),
                                client_id);
  if (initial_decode_failed)
    return false;

  if (decode_to_f16_using_n32_intermediate) {
    return ImageDecodeCacheUtils::ScaleToHalfFloatPixmapUsingN32Intermediate(
        decode_pixmap, &pixmap, filter_quality);
  }
  if (do_yuv_decode) {
    SkPixmap unscaled_pixmap_y;
    SkPixmap unscaled_pixmap_u;
    SkPixmap unscaled_pixmap_v;
    SetYuvPixmapsFromSizeInfo(&unscaled_pixmap_y, &unscaled_pixmap_u,
                              &unscaled_pixmap_v, yuva_size_info, planes,
                              decode_info, decode_pixmap.writable_addr());

    const SkImageInfo y_info_scaled = info.makeColorType(kGray_8_SkColorType);
    // The target raster dimensions get passed through:
    // |target_pixmap|.info() -> |pixmap|->info() -> |info| -> |y_info_scaled|
    const gfx::Size target_raster_size(y_info_scaled.width(),
                                       y_info_scaled.height());
    gfx::Size unscaled_u_size(unscaled_pixmap_u.width(),
                              unscaled_pixmap_u.height());
    gfx::Size unscaled_v_size(unscaled_pixmap_v.width(),
                              unscaled_pixmap_v.height());
    gfx::Size scaled_u_size;
    gfx::Size scaled_v_size;
    ComputeMippedUVPlaneSizes(target_raster_size, unscaled_u_size,
                              unscaled_v_size, draw_image, &scaled_u_size,
                              &scaled_v_size);
    const SkImageInfo u_info_scaled =
        y_info_scaled.makeWH(scaled_u_size.width(), scaled_u_size.height());
    const SkImageInfo v_info_scaled =
        y_info_scaled.makeWH(scaled_v_size.width(), scaled_v_size.height());
    const size_t y_plane_bytes = y_info_scaled.computeMinByteSize();
    const size_t u_plane_bytes = u_info_scaled.computeMinByteSize();
    DCHECK(!SkImageInfo::ByteSizeOverflowed(y_plane_bytes));
    DCHECK(!SkImageInfo::ByteSizeOverflowed(u_plane_bytes));

    pixmap_y->reset(y_info_scaled, data_ptr, y_info_scaled.minRowBytes());
    pixmap_u->reset(u_info_scaled, data_ptr + y_plane_bytes,
                    u_info_scaled.minRowBytes());
    pixmap_v->reset(v_info_scaled, data_ptr + y_plane_bytes + u_plane_bytes,
                    v_info_scaled.minRowBytes());

    const bool all_planes_scaled_successfully =
        unscaled_pixmap_y.scalePixels(*pixmap_y, filter_quality) &&
        unscaled_pixmap_u.scalePixels(*pixmap_u, filter_quality) &&
        unscaled_pixmap_v.scalePixels(*pixmap_v, filter_quality);
    return all_planes_scaled_successfully;
  }
  return decode_pixmap.scalePixels(pixmap, filter_quality);
}

// Takes ownership of the backing texture of an SkImage. This allows us to
// delete this texture under Skia (via discardable).
sk_sp<SkImage> TakeOwnershipOfSkImageBacking(GrContext* context,
                                             sk_sp<SkImage> image) {
  // If the image is not texture backed, it has no backing, just return it.
  if (!image->isTextureBacked()) {
    return image;
  }

  GrSurfaceOrigin origin;
  image->getBackendTexture(false /* flushPendingGrContextIO */, &origin);
  SkColorType color_type = image->colorType();
  if (color_type == kUnknown_SkColorType) {
    return nullptr;
  }
  sk_sp<SkColorSpace> color_space = image->refColorSpace();
  GrBackendTexture backend_texture;
  SkImage::BackendTextureReleaseProc release_proc;
  SkImage::MakeBackendTextureFromSkImage(context, std::move(image),
                                         &backend_texture, &release_proc);
  return SkImage::MakeFromTexture(context, backend_texture, origin, color_type,
                                  kPremul_SkAlphaType, std::move(color_space));
}

// Immediately deletes an SkImage, preventing caching of that image. Must be
// called while holding the context lock.
void DeleteSkImageAndPreventCaching(viz::RasterContextProvider* context,
                                    sk_sp<SkImage>&& image) {
  // No need to do anything for a non-texture-backed images.
  if (!image->isTextureBacked())
    return;

  sk_sp<SkImage> image_owned =
      TakeOwnershipOfSkImageBacking(context->GrContext(), std::move(image));
  // If context is lost, we may get a null image here.
  if (image_owned) {
    // Delete |original_image_owned| as Skia will not clean it up. We are
    // holding the context lock here, so we can delete immediately.
    uint32_t texture_id =
        GpuImageDecodeCache::GlIdFromSkImage(image_owned.get());
    context->RasterInterface()->DeleteGpuRasterTexture(texture_id);
  }
}

// TODO(ericrk): Replace calls to this with calls to SkImage::makeTextureImage,
// once that function handles colorspaces. https://crbug.com/834837
sk_sp<SkImage> MakeTextureImage(viz::RasterContextProvider* context,
                                sk_sp<SkImage> source_image,
                                sk_sp<SkColorSpace> target_color_space,
                                GrMipMapped mip_mapped) {
  // Step 1: Upload image and generate mips if necessary. If we will be applying
  // a color-space conversion, don't generate mips yet, instead do it after
  // conversion, in step 3.
  bool add_mips_after_color_conversion =
      (target_color_space && mip_mapped == GrMipMapped::kYes);
  sk_sp<SkImage> uploaded_image = source_image->makeTextureImage(
      context->GrContext(),
      add_mips_after_color_conversion ? GrMipMapped::kNo : mip_mapped);

  // Step 2: Apply a color-space conversion if necessary.
  if (uploaded_image && target_color_space) {
    sk_sp<SkImage> pre_converted_image = uploaded_image;
    uploaded_image = uploaded_image->makeColorSpace(target_color_space);

    if (uploaded_image != pre_converted_image)
      DeleteSkImageAndPreventCaching(context, std::move(pre_converted_image));
  }

  // Step 3: If we had a colorspace conversion, we couldn't mipmap in step 1, so
  // add mips here.
  if (uploaded_image && add_mips_after_color_conversion) {
    sk_sp<SkImage> pre_mipped_image = uploaded_image;
    uploaded_image = uploaded_image->makeTextureImage(context->GrContext(),
                                                      GrMipMapped::kYes);
    DCHECK_NE(pre_mipped_image, uploaded_image);
    DeleteSkImageAndPreventCaching(context, std::move(pre_mipped_image));
  }

  return uploaded_image;
}

size_t GetUploadedTextureSizeFromSkImage(const sk_sp<SkImage>& plane,
                                         const GrMipMapped mipped) {
  const size_t plane_size = GrContext::ComputeImageSize(plane, mipped);
  return plane_size;
}

}  // namespace

// static
GpuImageDecodeCache::InUseCacheKey
GpuImageDecodeCache::InUseCacheKey::FromDrawImage(const DrawImage& draw_image) {
  return InUseCacheKey(draw_image);
}

// Extract the information to uniquely identify a DrawImage for the purposes of
// the |in_use_cache_|.
GpuImageDecodeCache::InUseCacheKey::InUseCacheKey(const DrawImage& draw_image)
    : frame_key(draw_image.frame_key()),
      upload_scale_mip_level(CalculateUploadScaleMipLevel(draw_image)),
      filter_quality(CalculateDesiredFilterQuality(draw_image)),
      target_color_space(draw_image.target_color_space()) {}

bool GpuImageDecodeCache::InUseCacheKey::operator==(
    const InUseCacheKey& other) const {
  return frame_key == other.frame_key &&
         upload_scale_mip_level == other.upload_scale_mip_level &&
         filter_quality == other.filter_quality &&
         target_color_space == other.target_color_space;
}

size_t GpuImageDecodeCache::InUseCacheKeyHash::operator()(
    const InUseCacheKey& cache_key) const {
  return base::HashInts(
      cache_key.target_color_space.GetHash(),
      base::HashInts(cache_key.frame_key.hash(),
                     base::HashInts(cache_key.upload_scale_mip_level,
                                    cache_key.filter_quality)));
}

GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(
    scoped_refptr<ImageData> image_data)
    : image_data(std::move(image_data)) {}
GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(const InUseCacheEntry&) =
    default;
GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(InUseCacheEntry&&) =
    default;
GpuImageDecodeCache::InUseCacheEntry::~InUseCacheEntry() = default;

// Task which decodes an image and stores the result in discardable memory.
// This task does not use GPU resources and can be run on any thread.
class GpuImageDecodeTaskImpl : public TileTask {
 public:
  GpuImageDecodeTaskImpl(GpuImageDecodeCache* cache,
                         const DrawImage& draw_image,
                         const ImageDecodeCache::TracingInfo& tracing_info,
                         GpuImageDecodeCache::DecodeTaskType task_type)
      : TileTask(true),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info),
        task_type_(task_type) {
    DCHECK(!SkipImage(draw_image));
  }
  GpuImageDecodeTaskImpl(const GpuImageDecodeTaskImpl&) = delete;

  GpuImageDecodeTaskImpl& operator=(const GpuImageDecodeTaskImpl&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "GpuImageDecodeTaskImpl::RunOnWorkerThread", "mode",
                 "gpu", "source_prepare_tiles_id",
                 tracing_info_.prepare_tiles_id);

    const auto* image_metadata = image_.paint_image().GetImageHeaderMetadata();
    const ImageType image_type =
        image_metadata ? image_metadata->image_type : ImageType::kInvalid;
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        &image_.paint_image(),
        devtools_instrumentation::ScopedImageDecodeTask::kGpu,
        ImageDecodeCache::ToScopedTaskType(tracing_info_.task_type),
        ImageDecodeCache::ToScopedImageType(image_type));
    cache_->DecodeImageInTask(image_, tracing_info_.task_type);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageDecodeTaskCompleted(image_, task_type_);
  }

 protected:
  ~GpuImageDecodeTaskImpl() override = default;

 private:
  GpuImageDecodeCache* cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;
  const GpuImageDecodeCache::DecodeTaskType task_type_;
};

// Task which creates an image from decoded data. Typically this involves
// uploading data to the GPU, which requires this task be run on the non-
// concurrent thread.
class ImageUploadTaskImpl : public TileTask {
 public:
  ImageUploadTaskImpl(GpuImageDecodeCache* cache,
                      const DrawImage& draw_image,
                      scoped_refptr<TileTask> decode_dependency,
                      const ImageDecodeCache::TracingInfo& tracing_info)
      : TileTask(false),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info) {
    DCHECK(!SkipImage(draw_image));
    // If an image is already decoded and locked, we will not generate a
    // decode task.
    if (decode_dependency)
      dependencies_.push_back(std::move(decode_dependency));
  }
  ImageUploadTaskImpl(const ImageUploadTaskImpl&) = delete;

  ImageUploadTaskImpl& operator=(const ImageUploadTaskImpl&) = delete;

  // Override from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageUploadTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", tracing_info_.prepare_tiles_id);
    const auto* image_metadata = image_.paint_image().GetImageHeaderMetadata();
    const ImageType image_type =
        image_metadata ? image_metadata->image_type : ImageType::kInvalid;
    devtools_instrumentation::ScopedImageUploadTask image_upload_task(
        &image_.paint_image(), ImageDecodeCache::ToScopedImageType(image_type));
    cache_->UploadImageInTask(image_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageUploadTaskCompleted(image_);
  }

 protected:
  ~ImageUploadTaskImpl() override = default;

 private:
  GpuImageDecodeCache* cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;
};

GpuImageDecodeCache::ImageDataBase::ImageDataBase() = default;
GpuImageDecodeCache::ImageDataBase::~ImageDataBase() = default;

void GpuImageDecodeCache::ImageDataBase::OnSetLockedData(bool out_of_raster) {
  DCHECK_EQ(usage_stats_.lock_count, 1);
  DCHECK(!is_locked_);
  usage_stats_.first_lock_out_of_raster = out_of_raster;
  is_locked_ = true;
}

void GpuImageDecodeCache::ImageDataBase::OnResetData() {
  is_locked_ = false;
  usage_stats_ = UsageStats();
}

void GpuImageDecodeCache::ImageDataBase::OnLock() {
  DCHECK(!is_locked_);
  is_locked_ = true;
  ++usage_stats_.lock_count;
}

void GpuImageDecodeCache::ImageDataBase::OnUnlock() {
  DCHECK(is_locked_);
  is_locked_ = false;
  if (usage_stats_.lock_count == 1)
    usage_stats_.first_lock_wasted = !usage_stats_.used;
}

int GpuImageDecodeCache::ImageDataBase::UsageState() const {
  ImageUsageState state = IMAGE_USAGE_STATE_WASTED_ONCE;
  if (usage_stats_.lock_count == 1) {
    if (usage_stats_.used)
      state = IMAGE_USAGE_STATE_USED_ONCE;
    else
      state = IMAGE_USAGE_STATE_WASTED_ONCE;
  } else {
    if (usage_stats_.used)
      state = IMAGE_USAGE_STATE_USED_RELOCKED;
    else
      state = IMAGE_USAGE_STATE_WASTED_RELOCKED;
  }

  return state;
}

GpuImageDecodeCache::DecodedImageData::DecodedImageData(
    bool is_bitmap_backed,
    bool can_do_hardware_accelerated_decode,
    bool do_hardware_accelerated_decode)
    : is_bitmap_backed_(is_bitmap_backed),
      can_do_hardware_accelerated_decode_(can_do_hardware_accelerated_decode),
      do_hardware_accelerated_decode_(do_hardware_accelerated_decode) {}
GpuImageDecodeCache::DecodedImageData::~DecodedImageData() {
  ResetData();
}

bool GpuImageDecodeCache::DecodedImageData::Lock() {
  if (data_->Lock())
    OnLock();
  return is_locked_;
}

void GpuImageDecodeCache::DecodedImageData::Unlock() {
  data_->Unlock();
  OnUnlock();
}

void GpuImageDecodeCache::DecodedImageData::SetLockedData(
    std::unique_ptr<base::DiscardableMemory> data,
    sk_sp<SkImage> image,
    bool out_of_raster) {
  DCHECK(data);
  DCHECK(!data_);
  DCHECK(image);
  DCHECK(!image_);
  data_ = std::move(data);
  image_ = std::move(image);
  OnSetLockedData(out_of_raster);
}

void GpuImageDecodeCache::DecodedImageData::SetLockedData(
    std::unique_ptr<base::DiscardableMemory> data,
    sk_sp<SkImage> image_y,
    sk_sp<SkImage> image_u,
    sk_sp<SkImage> image_v,
    bool out_of_raster) {
  DCHECK(data);
  DCHECK(!data_);
  DCHECK(image_y);
  DCHECK(image_u);
  DCHECK(image_v);
  DCHECK(!image_yuv_planes_);
  data_ = std::move(data);
  image_yuv_planes_ = std::array<sk_sp<SkImage>, SkYUVASizeInfo::kMaxCount>();
  image_yuv_planes_->at(SkYUVAIndex::kY_Index) = std::move(image_y);
  image_yuv_planes_->at(SkYUVAIndex::kU_Index) = std::move(image_u);
  image_yuv_planes_->at(SkYUVAIndex::kV_Index) = std::move(image_v);
  OnSetLockedData(out_of_raster);
}

void GpuImageDecodeCache::DecodedImageData::SetBitmapImage(
    sk_sp<SkImage> image) {
  DCHECK(is_bitmap_backed_);
  image_ = std::move(image);
  OnLock();
}

void GpuImageDecodeCache::DecodedImageData::ResetBitmapImage() {
  DCHECK(is_bitmap_backed_);
  image_ = nullptr;
  image_yuv_planes_.reset();
  OnUnlock();
}

void GpuImageDecodeCache::DecodedImageData::ResetData() {
  if (data_) {
    if (is_yuv()) {
      DCHECK(image_yuv_planes_);
      DCHECK(image_yuv_planes_->at(SkYUVAIndex::kY_Index));
      DCHECK(image_yuv_planes_->at(SkYUVAIndex::kU_Index));
      DCHECK(image_yuv_planes_->at(SkYUVAIndex::kV_Index));
    } else {
      DCHECK(image_);
    }
    ReportUsageStats();
  }
  image_ = nullptr;
  image_yuv_planes_.reset();
  data_ = nullptr;
  OnResetData();
}

void GpuImageDecodeCache::DecodedImageData::ReportUsageStats() const {
  if (do_hardware_accelerated_decode_) {
    // When doing hardware decode acceleration, we don't want to record usage
    // stats for the decode data. The reason is that the decode is done in the
    // GPU process and the decoded result stays there. On the renderer side, we
    // don't use or lock the decoded data, so reporting this status would
    // incorrectly distort the software decoding statistics.
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Renderer4.GpuImageDecodeState",
                            static_cast<ImageUsageState>(UsageState()),
                            IMAGE_USAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageDecodeState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
  if (usage_stats_.first_lock_out_of_raster)
    UMA_HISTOGRAM_BOOLEAN(
        "Renderer4.GpuImageDecodeState.FirstLockWasted.OutOfRaster",
        usage_stats_.first_lock_wasted);
}

GpuImageDecodeCache::UploadedImageData::UploadedImageData() = default;
GpuImageDecodeCache::UploadedImageData::~UploadedImageData() {
  DCHECK(!image());
  DCHECK(!image_yuv_planes_);
  DCHECK(!gl_plane_ids_);
}

void GpuImageDecodeCache::UploadedImageData::SetImage(
    sk_sp<SkImage> image,
    bool represents_yuv_image) {
  DCHECK(mode_ == Mode::kNone);
  DCHECK(!image_);
  DCHECK(!transfer_cache_id_);
  DCHECK(image);

  mode_ = Mode::kSkImage;
  image_ = std::move(image);
  // Calling isTexturedBacked() on the YUV SkImage would flatten it to RGB.
  if (!represents_yuv_image && image_->isTextureBacked()) {
    gl_id_ = GlIdFromSkImage(image_.get());
  } else {
    gl_id_ = 0;
  }
  OnSetLockedData(false /* out_of_raster */);
}

void GpuImageDecodeCache::UploadedImageData::SetYuvImage(
    sk_sp<SkImage> y_image_input,
    sk_sp<SkImage> u_image_input,
    sk_sp<SkImage> v_image_input) {
  DCHECK(!image_yuv_planes_);
  DCHECK(!gl_plane_ids_);
  DCHECK(!transfer_cache_id_);
  DCHECK(y_image_input);
  DCHECK(u_image_input);
  DCHECK(v_image_input);

  mode_ = Mode::kSkImage;
  image_yuv_planes_ = std::array<sk_sp<SkImage>, SkYUVASizeInfo::kMaxCount>();
  image_yuv_planes_->at(SkYUVAIndex::kY_Index) = std::move(y_image_input);
  image_yuv_planes_->at(SkYUVAIndex::kU_Index) = std::move(u_image_input);
  image_yuv_planes_->at(SkYUVAIndex::kV_Index) = std::move(v_image_input);
  if (y_image()->isTextureBacked() && u_image()->isTextureBacked() &&
      v_image()->isTextureBacked()) {
    gl_plane_ids_ = std::array<GrGLuint, SkYUVASizeInfo::kMaxCount>();
    gl_plane_ids_->at(SkYUVAIndex::kY_Index) = GlIdFromSkImage(y_image().get());
    gl_plane_ids_->at(SkYUVAIndex::kU_Index) = GlIdFromSkImage(u_image().get());
    gl_plane_ids_->at(SkYUVAIndex::kV_Index) = GlIdFromSkImage(v_image().get());
  }
}

void GpuImageDecodeCache::UploadedImageData::SetTransferCacheId(uint32_t id) {
  DCHECK(mode_ == Mode::kNone);
  DCHECK(!image_);
  DCHECK(!transfer_cache_id_);

  mode_ = Mode::kTransferCache;
  transfer_cache_id_ = id;
  OnSetLockedData(false /* out_of_raster */);
}

void GpuImageDecodeCache::UploadedImageData::Reset() {
  if (mode_ != Mode::kNone)
    ReportUsageStats();
  mode_ = Mode::kNone;
  image_ = nullptr;
  image_yuv_planes_.reset();
  gl_plane_ids_.reset();
  gl_id_ = 0;
  is_alpha_ = false;
  transfer_cache_id_.reset();
  OnResetData();
}

void GpuImageDecodeCache::UploadedImageData::ReportUsageStats() const {
  UMA_HISTOGRAM_ENUMERATION("Renderer4.GpuImageUploadState",
                            static_cast<ImageUsageState>(UsageState()),
                            IMAGE_USAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageUploadState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
}

GpuImageDecodeCache::ImageData::ImageData(
    PaintImage::Id paint_image_id,
    DecodedDataMode mode,
    size_t size,
    const gfx::ColorSpace& target_color_space,
    SkFilterQuality quality,
    int upload_scale_mip_level,
    bool needs_mips,
    bool is_bitmap_backed,
    bool can_do_hardware_accelerated_decode,
    bool do_hardware_accelerated_decode,
    bool is_yuv_format,
    SkYUVColorSpace yuv_cs)
    : paint_image_id(paint_image_id),
      mode(mode),
      size(size),
      target_color_space(target_color_space),
      quality(quality),
      upload_scale_mip_level(upload_scale_mip_level),
      needs_mips(needs_mips),
      is_bitmap_backed(is_bitmap_backed),
      is_yuv(is_yuv_format),
      decode(is_bitmap_backed,
             can_do_hardware_accelerated_decode,
             do_hardware_accelerated_decode) {
  // Only fill out the base::Optional |yuv_color_space| if doing YUV decoding.
  // Otherwise it was filled out with a default "identity" value by the decoder.
  if (is_yuv) {
    DCHECK_LE(yuv_cs, SkYUVColorSpace::kLastEnum_SkYUVColorSpace);
    yuv_color_space = yuv_cs;
  }
}

GpuImageDecodeCache::ImageData::~ImageData() {
  // We should never delete ImageData while it is in use or before it has been
  // cleaned up.
  DCHECK_EQ(0u, upload.ref_count);
  DCHECK_EQ(0u, decode.ref_count);
  DCHECK_EQ(false, decode.is_locked());
  // This should always be cleaned up before deleting the image, as it needs to
  // be freed with the GL context lock held.
  DCHECK(!HasUploadedData());
}

bool GpuImageDecodeCache::ImageData::IsGpuOrTransferCache() const {
  return mode == DecodedDataMode::kGpu ||
         mode == DecodedDataMode::kTransferCache;
}

bool GpuImageDecodeCache::ImageData::HasUploadedData() const {
  switch (mode) {
    case DecodedDataMode::kGpu:
      // upload.image() stores the result of MakeFromYUVATextures
      if (upload.image()) {
        // TODO(915968): Be smarter about being able to re-upload planes
        // selectively if only some get deleted from under us.
        DCHECK(!is_yuv || upload.has_yuv_planes());
        return true;
      }
      return false;
    case DecodedDataMode::kTransferCache:
      return !!upload.transfer_cache_id();
    case DecodedDataMode::kCpu:
      return false;
  }
  return false;
}

void GpuImageDecodeCache::ImageData::ValidateBudgeted() const {
  // If the image is budgeted, it must be refed.
  DCHECK(is_budgeted);
  DCHECK_GT(upload.ref_count, 0u);
}

// static
GrGLuint GpuImageDecodeCache::GlIdFromSkImage(const SkImage* image) {
  DCHECK(image->isTextureBacked());
  GrBackendTexture backend_texture =
      image->getBackendTexture(true /* flushPendingGrContextIO */);
  if (!backend_texture.isValid())
    return 0;

  GrGLTextureInfo info;
  if (!backend_texture.getGLTextureInfo(&info))
    return 0;

  return info.fID;
}

GpuImageDecodeCache::GpuImageDecodeCache(
    viz::RasterContextProvider* context,
    bool use_transfer_cache,
    SkColorType color_type,
    size_t max_working_set_bytes,
    int max_texture_size,
    PaintImage::GeneratorClientId generator_client_id)
    : color_type_(color_type),
      use_transfer_cache_(use_transfer_cache),
      context_(context),
      max_texture_size_(max_texture_size),
      generator_client_id_(generator_client_id),
      persistent_cache_(PersistentCache::NO_AUTO_EVICT),
      max_working_set_bytes_(max_working_set_bytes),
      max_working_set_items_(kMaxItemsInWorkingSet) {
  // Note that to compute |allow_accelerated_jpeg_decodes_| and
  // |allow_accelerated_webp_decodes_|, the last thing we check is the feature
  // flag. That's because we want to ensure that we're in OOP-R mode and the
  // hardware decoder supports the image type so that finch experiments
  // involving hardware decode acceleration only count users in that
  // population (both in the 'control' and the 'enabled' groups).
  allow_accelerated_jpeg_decodes_ =
      use_transfer_cache &&
      context_->ContextSupport()->IsJpegDecodeAccelerationSupported() &&
      base::FeatureList::IsEnabled(features::kVaapiJpegImageDecodeAcceleration);
  allow_accelerated_webp_decodes_ =
      use_transfer_cache &&
      context_->ContextSupport()->IsWebPDecodeAccelerationSupported() &&
      base::FeatureList::IsEnabled(features::kVaapiWebPImageDecodeAcceleration);
  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::GpuImageDecodeCache", base::ThreadTaskRunnerHandle::Get());
  }
  memory_pressure_listener_.reset(
      new base::MemoryPressureListener(base::BindRepeating(
          &GpuImageDecodeCache::OnMemoryPressure, base::Unretained(this))));
}

GpuImageDecodeCache::~GpuImageDecodeCache() {
  // Debugging crbug.com/650234.
  CHECK_EQ(0u, in_use_cache_.size());

  // SetShouldAggressivelyFreeResources will zero our limits and free all
  // outstanding image memory.
  SetShouldAggressivelyFreeResources(true);

  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

ImageDecodeCache::TaskResult GpuImageDecodeCache::GetTaskForImageAndRef(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info) {
  DCHECK_EQ(tracing_info.task_type, TaskType::kInRaster);
  return GetTaskForImageAndRefInternal(draw_image, tracing_info,
                                       DecodeTaskType::kPartOfUploadTask);
}

ImageDecodeCache::TaskResult
GpuImageDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(
    const DrawImage& draw_image) {
  return GetTaskForImageAndRefInternal(
      draw_image, TracingInfo(0, TilePriority::NOW, TaskType::kOutOfRaster),
      DecodeTaskType::kStandAloneDecodeTask);
}

ImageDecodeCache::TaskResult GpuImageDecodeCache::GetTaskForImageAndRefInternal(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetTaskForImageAndRef");

  if (SkipImage(draw_image)) {
    return TaskResult(false /* need_unref */, false /* is_at_raster_decode */,
                      false /* can_do_hardware_accelerated_decode */);
  }

  base::AutoLock lock(lock_);
  const InUseCacheKey cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  scoped_refptr<ImageData> new_data;
  if (!image_data) {
    // We need an ImageData, create one now. Note that hardware decode
    // acceleration is allowed only in the DecodeTaskType::kPartOfUploadTask
    // case. This prevents the img.decode() and checkerboard images paths from
    // going through hardware decode acceleration.
    new_data = CreateImageData(
        draw_image,
        task_type ==
            DecodeTaskType::kPartOfUploadTask /* allow_hardware_decode */);
    image_data = new_data.get();
  } else if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image, so just return.
    return TaskResult(false /* need_unref */, false /* is_at_raster_decode */,
                      image_data->decode.can_do_hardware_accelerated_decode());
  } else if (task_type == DecodeTaskType::kPartOfUploadTask &&
             image_data->upload.task) {
    // We had an existing upload task, ref the image and return the task.
    image_data->ValidateBudgeted();
    RefImage(draw_image, cache_key);
    return TaskResult(image_data->upload.task,
                      image_data->decode.can_do_hardware_accelerated_decode());
  } else if (task_type == DecodeTaskType::kStandAloneDecodeTask &&
             image_data->decode.stand_alone_task) {
    // We had an existing out of raster task, ref the image and return the task.
    image_data->ValidateBudgeted();
    RefImage(draw_image, cache_key);
    DCHECK(!image_data->decode.can_do_hardware_accelerated_decode());
    return TaskResult(image_data->decode.stand_alone_task,
                      false /* can_do_hardware_accelerated_decode */);
  }

  // Ensure that the image we're about to decode/upload will fit in memory, if
  // not already budgeted.
  if (!image_data->is_budgeted && !EnsureCapacity(image_data->size)) {
    // Image will not fit, do an at-raster decode.
    return TaskResult(false /* need_unref */, true /* is_at_raster_decode */,
                      image_data->decode.can_do_hardware_accelerated_decode());
  }

  // If we had to create new image data, add it to our map now that we know it
  // will fit.
  if (new_data)
    AddToPersistentCache(draw_image, std::move(new_data));

  // Ref the image before creating a task - this ref is owned by the caller, and
  // it is their responsibility to release it by calling UnrefImage.
  RefImage(draw_image, cache_key);

  // If we already have an image and it is locked (or lock-able), just return
  // that. The image must be budgeted before we attempt to lock it.
  DCHECK(image_data->is_budgeted);
  if (image_data->HasUploadedData() &&
      TryLockImage(HaveContextLock::kNo, draw_image, image_data)) {
    return TaskResult(true /* need_unref */, false /* is_at_raster_decode */,
                      image_data->decode.can_do_hardware_accelerated_decode());
  }

  scoped_refptr<TileTask> task;
  if (task_type == DecodeTaskType::kPartOfUploadTask) {
    // Ref image and create a upload and decode tasks. We will release this ref
    // in UploadTaskCompleted.
    RefImage(draw_image, cache_key);
    task = base::MakeRefCounted<ImageUploadTaskImpl>(
        this, draw_image,
        GetImageDecodeTaskAndRef(draw_image, tracing_info, task_type),
        tracing_info);
    image_data->upload.task = task;
  } else {
    task = GetImageDecodeTaskAndRef(draw_image, tracing_info, task_type);
  }

  return TaskResult(task,
                    image_data->decode.can_do_hardware_accelerated_decode());
}

void GpuImageDecodeCache::UnrefImage(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImage");
  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image, InUseCacheKey::FromDrawImage(draw_image));
}

bool GpuImageDecodeCache::UseCacheForDrawImage(
    const DrawImage& draw_image) const {
  if (draw_image.paint_image().GetSkImage()->isTextureBacked())
    return false;

  return true;
}

DecodedDrawImage GpuImageDecodeCache::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetDecodedImageForDraw");

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  CheckContextLockAcquiredIfNecessary();

  // If we're skipping the image, then the filter quality doesn't matter.
  if (SkipImage(draw_image))
    return DecodedDrawImage();

  base::AutoLock lock(lock_);
  const InUseCacheKey cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  if (!image_data) {
    // We didn't find the image, create a new entry.
    auto data = CreateImageData(draw_image, true /* allow_hardware_decode */);
    image_data = data.get();
    AddToPersistentCache(draw_image, std::move(data));
  }

  // Ref the image and decode so that they stay alive while we are
  // decoding/uploading.
  // Note that refing the image will attempt to budget the image, if not already
  // done.
  RefImage(draw_image, cache_key);
  RefImageDecode(draw_image, cache_key);

  // We may or may not need to decode and upload the image we've found, the
  // following functions early-out to if we already decoded.
  DecodeImageIfNecessary(draw_image, image_data, TaskType::kInRaster);
  UploadImageIfNecessary(draw_image, image_data);
  // Unref the image decode, but not the image. The image ref will be released
  // in DrawWithImageFinished.
  UnrefImageDecode(draw_image, cache_key);

  if (image_data->mode == DecodedDataMode::kTransferCache) {
    DCHECK(use_transfer_cache_);
    auto id = image_data->upload.transfer_cache_id();
    if (id)
      image_data->upload.mark_used();
    DCHECK(id || image_data->decode.decode_failure);

    SkSize scale_factor = CalculateScaleFactorForMipLevel(
        draw_image, image_data->upload_scale_mip_level);
    DecodedDrawImage decoded_draw_image(
        id, SkSize(), scale_factor, CalculateDesiredFilterQuality(draw_image),
        image_data->needs_mips, image_data->is_budgeted);
    return decoded_draw_image;
  } else {
    DCHECK(!use_transfer_cache_);
    sk_sp<SkImage> image = image_data->upload.image();
    if (image)
      image_data->upload.mark_used();
    DCHECK(image || image_data->decode.decode_failure);

    SkSize scale_factor = CalculateScaleFactorForMipLevel(
        draw_image, image_data->upload_scale_mip_level);
    DecodedDrawImage decoded_draw_image(
        std::move(image), SkSize(), scale_factor,
        CalculateDesiredFilterQuality(draw_image), image_data->is_budgeted);
    return decoded_draw_image;
  }
}

void GpuImageDecodeCache::DrawWithImageFinished(
    const DrawImage& draw_image,
    const DecodedDrawImage& decoded_draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::DrawWithImageFinished");

  // Release decoded_draw_image to ensure the referenced SkImage can be
  // cleaned up below.
  { auto delete_decoded_draw_image = std::move(decoded_draw_image); }

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  CheckContextLockAcquiredIfNecessary();

  if (SkipImage(draw_image))
    return;

  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image, InUseCacheKey::FromDrawImage(draw_image));

  // We are mid-draw and holding the context lock, ensure we clean up any
  // textures (especially at-raster), which may have just been marked for
  // deletion by UnrefImage.
  RunPendingContextThreadOperations();
}

void GpuImageDecodeCache::ReduceCacheUsage() NO_THREAD_SAFETY_ANALYSIS {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::ReduceCacheUsage");
  base::AutoLock lock(lock_);
  EnsureCapacity(0);

  // This is typically called when no tasks are running (between scheduling
  // tasks). Try to lock and run pending operations if possible, but don't
  // block on it.
  //
  // NO_THREAD_SAFETY_ANALYSIS: runtime-dependent locking.
  if (context_->GetLock() && !context_->GetLock()->Try())
    return;

  RunPendingContextThreadOperations();
  if (context_->GetLock())
    context_->GetLock()->Release();
}

void GpuImageDecodeCache::SetShouldAggressivelyFreeResources(
    bool aggressively_free_resources) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::SetShouldAggressivelyFreeResources",
               "agressive_free_resources", aggressively_free_resources);
  if (aggressively_free_resources) {
    base::Optional<viz::RasterContextProvider::ScopedRasterContextLock>
        context_lock;
    if (context_->GetLock())
      context_lock.emplace(context_);

    base::AutoLock lock(lock_);
    aggressively_freeing_resources_ = aggressively_free_resources;
    EnsureCapacity(0);

    // We are holding the context lock, so finish cleaning up deleted images
    // now.
    RunPendingContextThreadOperations();
  } else {
    base::AutoLock lock(lock_);
    aggressively_freeing_resources_ = aggressively_free_resources;
  }
}

void GpuImageDecodeCache::ClearCache() {
  base::AutoLock lock(lock_);
  for (auto it = persistent_cache_.begin(); it != persistent_cache_.end();)
    it = RemoveFromPersistentCache(it);
  DCHECK(persistent_cache_.empty());
  paint_image_entries_.clear();
}

void GpuImageDecodeCache::RecordStats() {
  base::AutoLock lock(lock_);
  double cache_usage;
  if (working_set_bytes_ > 0 &&
      base::CheckDiv(static_cast<double>(working_set_bytes_),
                     max_working_set_bytes_)
          .AssignIfValid(&cache_usage)) {
    UMA_HISTOGRAM_PERCENTAGE(
        "Renderer4.GpuImageDecodeState.CachePeakUsagePercent",
        cache_usage * 100);
  }
}

void GpuImageDecodeCache::AddToPersistentCache(const DrawImage& draw_image,
                                               scoped_refptr<ImageData> data) {
  lock_.AssertAcquired();

  WillAddCacheEntry(draw_image);
  persistent_cache_.Put(draw_image.frame_key(), std::move(data));
}

template <typename Iterator>
Iterator GpuImageDecodeCache::RemoveFromPersistentCache(Iterator it) {
  lock_.AssertAcquired();

  if (it->second->decode.ref_count != 0 || it->second->upload.ref_count != 0) {
    // Orphan the image and erase it from the |persisent_cache_|. This ensures
    // that the image will be deleted once all refs are removed.
    it->second->is_orphaned = true;
  } else {
    // Current entry has no refs. Ensure it is not locked.
    DCHECK(!it->second->decode.is_locked());
    DCHECK(!it->second->upload.is_locked());

    // Unlocked images must not be budgeted.
    DCHECK(!it->second->is_budgeted);

    // Free the uploaded image if it exists.
    if (it->second->HasUploadedData())
      DeleteImage(it->second.get());
  }

  auto entries_it = paint_image_entries_.find(it->second->paint_image_id);
  DCHECK(entries_it != paint_image_entries_.end());
  DCHECK_GT(entries_it->second.count, 0u);

  // If this is the last entry for this image, remove its tracking.
  --entries_it->second.count;
  if (entries_it->second.count == 0u)
    paint_image_entries_.erase(entries_it);

  return persistent_cache_.Erase(it);
}

size_t GpuImageDecodeCache::GetMaximumMemoryLimitBytes() const {
  return max_working_set_bytes_;
}

void GpuImageDecodeCache::AddTextureDump(
    base::trace_event::ProcessMemoryDump* pmd,
    const std::string& texture_dump_name,
    const size_t bytes,
    const GrGLuint gl_id,
    const size_t locked_size) const {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryAllocatorDumpGuid;

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(texture_dump_name);
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, bytes);

  // Dump the "locked_size" as an additional column.
  dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes, locked_size);

  MemoryAllocatorDumpGuid guid;
  guid = gl::GetGLTextureClientGUIDForTracing(
      context_->ContextSupport()->ShareGroupTracingGUID(), gl_id);
  pmd->CreateSharedGlobalAllocatorDump(guid);
  // Importance of 3 gives this dump priority over the dump made by Skia
  // (importance 2), attributing memory here.
  const int kImportance = 3;
  pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
}

void GpuImageDecodeCache::MemoryDumpYUVImage(
    base::trace_event::ProcessMemoryDump* pmd,
    const ImageData* image_data,
    const std::string& dump_base_name,
    size_t locked_size) const {
  using base::trace_event::MemoryAllocatorDump;
  DCHECK(image_data->is_yuv);
  DCHECK(image_data->upload.has_yuv_planes());

  struct PlaneMemoryDumpInfo {
    size_t byte_size;
    GrGLuint gl_id;
  };
  std::vector<PlaneMemoryDumpInfo> plane_dump_infos;
  const GrMipMapped mipped =
      image_data->needs_mips ? GrMipMapped::kYes : GrMipMapped::kNo;
  // TODO(crbug.com/910276): Also include alpha plane if applicable.
  plane_dump_infos.push_back(
      {GetUploadedTextureSizeFromSkImage(image_data->upload.y_image(), mipped),
       image_data->upload.gl_y_id()});
  plane_dump_infos.push_back(
      {GetUploadedTextureSizeFromSkImage(image_data->upload.u_image(), mipped),
       image_data->upload.gl_u_id()});
  plane_dump_infos.push_back(
      {GetUploadedTextureSizeFromSkImage(image_data->upload.v_image(), mipped),
       image_data->upload.gl_v_id()});

  for (size_t i = 0u; i < plane_dump_infos.size(); ++i) {
    auto plane_dump_info = plane_dump_infos.at(i);
    // If the image is currently locked, we dump the locked size per plane.
    AddTextureDump(
        pmd,
        dump_base_name +
            base::StringPrintf("/plane_%0u", base::checked_cast<uint32_t>(i)),
        plane_dump_info.byte_size, plane_dump_info.gl_id,
        locked_size ? plane_dump_info.byte_size : 0u);
  }
}

bool GpuImageDecodeCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryAllocatorDumpGuid;
  using base::trace_event::MemoryDumpLevelOfDetail;

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnMemoryDump");

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name = base::StringPrintf(
        "cc/image_memory/cache_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, working_set_bytes_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& image_pair : persistent_cache_) {
    const ImageData* image_data = image_pair.second.get();
    int image_id = static_cast<int>(image_pair.first.hash());

    // If we have discardable decoded data, dump this here.
    if (image_data->decode.data()) {
      std::string discardable_dump_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/discardable/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      MemoryAllocatorDump* dump =
          image_data->decode.data()->CreateMemoryAllocatorDump(
              discardable_dump_name.c_str(), pmd);
      // Dump the "locked_size" as an additional column.
      // This lets us see the amount of discardable which is contributing to
      // memory pressure.
      size_t locked_size =
          image_data->decode.is_locked() ? image_data->size : 0u;
      dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                      locked_size);
    }

    // If we have an uploaded image (that is actually on the GPU, not just a
    // CPU wrapper), upload it here.
    if (image_data->HasUploadedData() &&
        image_data->mode == DecodedDataMode::kGpu) {
      size_t discardable_size = image_data->size;
      auto* context_support = context_->ContextSupport();
      // If the discardable system has deleted this out from under us, log a
      // size of 0 to match software discardable.
      if (image_data->is_yuv &&
          context_support->ThreadsafeDiscardableTextureIsDeletedForTracing(
              image_data->upload.gl_y_id()) &&
          context_support->ThreadsafeDiscardableTextureIsDeletedForTracing(
              image_data->upload.gl_u_id()) &&
          context_support->ThreadsafeDiscardableTextureIsDeletedForTracing(
              image_data->upload.gl_v_id())) {
        discardable_size = 0;
      } else if (context_support
                     ->ThreadsafeDiscardableTextureIsDeletedForTracing(
                         image_data->upload.gl_id())) {
        discardable_size = 0;
      }

      std::string gpu_dump_base_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/gpu/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      size_t locked_size =
          image_data->upload.is_locked() ? discardable_size : 0u;
      if (image_data->is_yuv) {
        MemoryDumpYUVImage(pmd, image_data, gpu_dump_base_name, locked_size);
      } else {
        AddTextureDump(pmd, gpu_dump_base_name, discardable_size,
                       image_data->upload.gl_id(), locked_size);
      }
    }
  }

  return true;
}

void GpuImageDecodeCache::DecodeImageInTask(const DrawImage& draw_image,
                                            TaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::DecodeImage");
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(
      draw_image, InUseCacheKey::FromDrawImage(draw_image));
  DCHECK(image_data);
  DCHECK(image_data->is_budgeted) << "Must budget an image for pre-decoding";
  DecodeImageIfNecessary(draw_image, image_data, task_type);
}

void GpuImageDecodeCache::UploadImageInTask(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UploadImage");
  base::Optional<viz::RasterContextProvider::ScopedRasterContextLock>
      context_lock;
  if (context_->GetLock())
    context_lock.emplace(context_);

  base::Optional<ScopedGrContextAccess> gr_context_access;
  if (!use_transfer_cache_)
    gr_context_access.emplace(context_);
  base::AutoLock lock(lock_);

  auto cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  DCHECK(image_data->is_budgeted) << "Must budget an image for pre-decoding";

  if (image_data->is_bitmap_backed)
    DecodeImageIfNecessary(draw_image, image_data, TaskType::kInRaster);
  UploadImageIfNecessary(draw_image, image_data);
}

void GpuImageDecodeCache::OnImageDecodeTaskCompleted(
    const DrawImage& draw_image,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageDecodeTaskCompleted");
  base::AutoLock lock(lock_);
  auto cache_key = InUseCacheKey::FromDrawImage(draw_image);
  // Decode task is complete, remove our reference to it.
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  if (task_type == DecodeTaskType::kPartOfUploadTask) {
    DCHECK(image_data->decode.task);
    image_data->decode.task = nullptr;
  } else {
    DCHECK(task_type == DecodeTaskType::kStandAloneDecodeTask);
    DCHECK(image_data->decode.stand_alone_task);
    image_data->decode.stand_alone_task = nullptr;
  }

  // While the decode task is active, we keep a ref on the decoded data.
  // Release that ref now.
  UnrefImageDecode(draw_image, cache_key);
}

void GpuImageDecodeCache::OnImageUploadTaskCompleted(
    const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageUploadTaskCompleted");
  base::AutoLock lock(lock_);
  // Upload task is complete, remove our reference to it.
  InUseCacheKey cache_key = InUseCacheKey::FromDrawImage(draw_image);
  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  DCHECK(image_data->upload.task);
  image_data->upload.task = nullptr;

  // While the upload task is active, we keep a ref on both the image it will be
  // populating, as well as the decode it needs to populate it. Release these
  // refs now.
  UnrefImageDecode(draw_image, cache_key);
  UnrefImageInternal(draw_image, cache_key);
}

// Checks if an image decode needs a decode task and returns it.
scoped_refptr<TileTask> GpuImageDecodeCache::GetImageDecodeTaskAndRef(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDecodeTaskAndRef");
  lock_.AssertAcquired();

  auto cache_key = InUseCacheKey::FromDrawImage(draw_image);

  // This ref is kept alive while an upload task may need this decode. We
  // release this ref in UploadTaskCompleted.
  if (task_type == DecodeTaskType::kPartOfUploadTask)
    RefImageDecode(draw_image, cache_key);

  ImageData* image_data = GetImageDataForDrawImage(draw_image, cache_key);
  DCHECK(image_data);
  if (image_data->decode.do_hardware_accelerated_decode())
    return nullptr;

  // No decode is necessary for bitmap backed images.
  if (image_data->decode.is_locked() || image_data->is_bitmap_backed) {
    // We should never be creating a decode task for a not budgeted image.
    DCHECK(image_data->is_budgeted);
    // We should never be creating a decode for an already-uploaded image.
    DCHECK(!image_data->HasUploadedData());
    return nullptr;
  }

  // We didn't have an existing locked image, create a task to lock or decode.
  scoped_refptr<TileTask>& existing_task =
      (task_type == DecodeTaskType::kPartOfUploadTask)
          ? image_data->decode.task
          : image_data->decode.stand_alone_task;
  if (!existing_task) {
    // Ref image decode and create a decode task. This ref will be released in
    // DecodeTaskCompleted.
    RefImageDecode(draw_image, cache_key);
    existing_task = base::MakeRefCounted<GpuImageDecodeTaskImpl>(
        this, draw_image, tracing_info, task_type);
  }
  return existing_task;
}

void GpuImageDecodeCache::RefImageDecode(const DrawImage& draw_image,
                                         const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::RefImageDecode");
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);
  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->decode.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageDecode(const DrawImage& draw_image,
                                           const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImageDecode");
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);
  DCHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->decode.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->decode.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

void GpuImageDecodeCache::RefImage(const DrawImage& draw_image,
                                   const InUseCacheKey& cache_key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::RefImage");
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);

  // If no secondary cache entry was found for the given |draw_image|, then
  // the draw_image only exists in the |persistent_cache_|. Create an in-use
  // cache entry now.
  if (found == in_use_cache_.end()) {
    auto found_image = persistent_cache_.Peek(draw_image.frame_key());
    DCHECK(found_image != persistent_cache_.end());
    DCHECK(IsCompatible(found_image->second.get(), draw_image));
    found = in_use_cache_
                .insert(InUseCache::value_type(
                    cache_key, InUseCacheEntry(found_image->second)))
                .first;
  }

  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->upload.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageInternal(const DrawImage& draw_image,
                                             const InUseCacheKey& cache_key) {
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(cache_key);
  DCHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->upload.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->upload.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

// Called any time an image or decode ref count changes. Takes care of any
// necessary memory budget book-keeping and cleanup.
void GpuImageDecodeCache::OwnershipChanged(const DrawImage& draw_image,
                                           ImageData* image_data) {
  lock_.AssertAcquired();

  bool has_any_refs =
      image_data->upload.ref_count > 0 || image_data->decode.ref_count > 0;
  // If we have no image refs on an image, we should unbudget it.
  if (!has_any_refs && image_data->is_budgeted) {
    DCHECK_GE(working_set_bytes_, image_data->size);
    DCHECK_GE(working_set_items_, 1u);
    working_set_bytes_ -= image_data->size;
    working_set_items_ -= 1;
    image_data->is_budgeted = false;
  }

  // Don't keep around completely empty images. This can happen if an image's
  // decode/upload tasks were both cancelled before completing.
  const bool has_cpu_data =
      image_data->decode.data() ||
      (image_data->is_bitmap_backed && image_data->decode.image());
  if (!has_any_refs && !image_data->HasUploadedData() && !has_cpu_data &&
      !image_data->is_orphaned) {
    auto found_persistent = persistent_cache_.Peek(draw_image.frame_key());
    if (found_persistent != persistent_cache_.end())
      RemoveFromPersistentCache(found_persistent);
  }

  // Don't keep discardable cpu memory for GPU backed images. The cache hit rate
  // of the cpu fallback (in case we don't find this image in gpu memory) is
  // too low to cache this data.
  if (image_data->decode.ref_count == 0 &&
      image_data->mode != DecodedDataMode::kCpu &&
      image_data->HasUploadedData()) {
    image_data->decode.ResetData();
  }

  // If we have no refs on an uploaded image, it should be unlocked. Do this
  // before any attempts to delete the image.
  if (image_data->IsGpuOrTransferCache() && image_data->upload.ref_count == 0 &&
      image_data->upload.is_locked()) {
    UnlockImage(image_data);
  }

  // Don't keep around orphaned images.
  if (image_data->is_orphaned && !has_any_refs) {
    DeleteImage(image_data);
  }

  // Don't keep CPU images if they are unused, these images can be recreated by
  // re-locking discardable (rather than requiring a full upload like GPU
  // images).
  if (image_data->mode == DecodedDataMode::kCpu && !has_any_refs) {
    DeleteImage(image_data);
  }

  // If we have image that could be budgeted, but isn't, budget it now.
  if (has_any_refs && !image_data->is_budgeted &&
      CanFitInWorkingSet(image_data->size)) {
    working_set_bytes_ += image_data->size;
    working_set_items_ += 1;
    image_data->is_budgeted = true;
  }

  // We should unlock the decoded image memory for the image in two cases:
  // 1) The image is no longer being used (no decode or upload refs).
  // 2) This is a non-CPU image that has already been uploaded and we have
  //    no remaining decode refs.
  bool should_unlock_decode = !has_any_refs || (image_data->HasUploadedData() &&
                                                !image_data->decode.ref_count);

  if (should_unlock_decode && image_data->decode.is_locked()) {
    if (image_data->is_bitmap_backed) {
      DCHECK(!image_data->decode.data());
      image_data->decode.ResetBitmapImage();
    } else {
      DCHECK(image_data->decode.data());
      image_data->decode.Unlock();
    }
  }

  // EnsureCapacity to make sure we are under our cache limits.
  EnsureCapacity(0);

#if DCHECK_IS_ON()
  // Sanity check the above logic.
  if (image_data->HasUploadedData()) {
    if (image_data->mode == DecodedDataMode::kCpu)
      DCHECK(image_data->decode.is_locked());
  } else {
    DCHECK(!image_data->is_budgeted || has_any_refs);
  }
#endif
}

// Checks whether we can fit a new image of size |required_size| in our
// working set. Also frees unreferenced entries to keep us below our preferred
// items limit.
bool GpuImageDecodeCache::EnsureCapacity(size_t required_size) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::EnsureCapacity");
  lock_.AssertAcquired();

  // While we are over preferred item capacity, we iterate through our set of
  // cached image data in LRU order, removing unreferenced images.
  for (auto it = persistent_cache_.rbegin();
       it != persistent_cache_.rend() && ExceedsPreferredCount();) {
    if (it->second->decode.ref_count != 0 ||
        it->second->upload.ref_count != 0) {
      ++it;
      continue;
    }

    it = RemoveFromPersistentCache(it);
  }

  return CanFitInWorkingSet(required_size);
}

bool GpuImageDecodeCache::CanFitInWorkingSet(size_t size) const {
  lock_.AssertAcquired();

  if (working_set_items_ >= max_working_set_items_)
    return false;

  base::CheckedNumeric<uint32_t> new_size(working_set_bytes_);
  new_size += size;
  if (!new_size.IsValid() || new_size.ValueOrDie() > max_working_set_bytes_)
    return false;

  return true;
}

bool GpuImageDecodeCache::ExceedsPreferredCount() const {
  lock_.AssertAcquired();

  size_t items_limit;
  if (aggressively_freeing_resources_) {
    items_limit = kSuspendedMaxItemsInCacheForGpu;
  } else {
    items_limit = kNormalMaxItemsInCacheForGpu;
  }

  return persistent_cache_.size() > items_limit;
}

void GpuImageDecodeCache::InsertTransferCacheEntry(
    const ClientImageTransferCacheEntry& image_entry,
    ImageData* image_data) {
  DCHECK(image_data);
  uint32_t size = image_entry.SerializedSize();
  void* data = context_->ContextSupport()->MapTransferCacheEntry(size);
  if (data) {
    bool succeeded = image_entry.Serialize(
        base::make_span(reinterpret_cast<uint8_t*>(data), size));
    DCHECK(succeeded);
    context_->ContextSupport()->UnmapAndCreateTransferCacheEntry(
        image_entry.UnsafeType(), image_entry.Id());
    image_data->upload.SetTransferCacheId(image_entry.Id());
  } else {
    // Transfer cache entry can fail due to a lost gpu context or failure
    // to allocate shared memory.  Handle this gracefully.  Mark this
    // image as "decode failed" so that we do not try to handle it again.
    // If this was a lost context, we'll recreate this image decode cache.
    image_data->decode.decode_failure = true;
  }
}

void GpuImageDecodeCache::DecodeImageIfNecessary(const DrawImage& draw_image,
                                                 ImageData* image_data,
                                                 TaskType task_type) {
  lock_.AssertAcquired();

  DCHECK_GT(image_data->decode.ref_count, 0u);

  if (image_data->decode.do_hardware_accelerated_decode()) {
    // We get here in the case of an at-raster decode.
    return;
  }

  if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image. Don't try again.
    return;
  }

  if (image_data->HasUploadedData() &&
      TryLockImage(HaveContextLock::kNo, draw_image, image_data)) {
    // We already have an uploaded image, no reason to decode.
    return;
  }

  if (image_data->is_bitmap_backed) {
    DCHECK(!draw_image.paint_image().IsLazyGenerated());
    if (image_data->is_yuv) {
      DLOG(ERROR) << "YUV + Bitmap is unknown and unimplemented!";
      NOTREACHED();
    } else {
      image_data->decode.SetBitmapImage(draw_image.paint_image().GetSkImage());
    }
    return;
  }

  if (image_data->decode.data() &&
      (image_data->decode.is_locked() || image_data->decode.Lock())) {
    // We already decoded this, or we just needed to lock, early out.
    return;
  }

  TRACE_EVENT0("cc,benchmark", "GpuImageDecodeCache::DecodeImage");

  image_data->decode.ResetData();
  std::unique_ptr<base::DiscardableMemory> backing_memory;
  sk_sp<SkImage> image;
  // These are used only for decoding into YUV.
  sk_sp<SkImage> image_y;
  sk_sp<SkImage> image_u;
  sk_sp<SkImage> image_v;
  {
    base::AutoUnlock unlock(lock_);
    auto* allocator = base::DiscardableMemoryAllocator::GetInstance();
    backing_memory = allocator->AllocateLockedDiscardableMemoryWithRetryOrDie(
        image_data->size, base::BindOnce(&GpuImageDecodeCache::ClearCache,
                                         base::Unretained(this)));
    sk_sp<SkColorSpace> color_space =
        ColorSpaceForImageDecode(draw_image, image_data->mode);
    auto release_proc = [](const void*, void*) {};
    SkImageInfo image_info = CreateImageInfoForDrawImage(
        draw_image, image_data->upload_scale_mip_level);
    SkPixmap pixmap(image_info, backing_memory->data(),
                    image_info.minRowBytes());

    // Set |pixmap| to the desired colorspace to decode into.
    pixmap.setColorSpace(color_space);

    if (image_data->is_yuv) {
      DVLOG(3) << "GpuImageDecodeCache wants to do YUV decoding/rendering";
      SkPixmap pixmap_y;
      SkPixmap pixmap_u;
      SkPixmap pixmap_v;
      if (!DrawAndScaleImage(draw_image, &pixmap, generator_client_id_,
                             image_data->is_yuv, &pixmap_y, &pixmap_u,
                             &pixmap_v)) {
        DLOG(ERROR) << "DrawAndScaleImage failed.";
        backing_memory->Unlock();
        backing_memory.reset();
      } else {
        image_y = SkImage::MakeFromRaster(pixmap_y, release_proc, nullptr);
        image_u = SkImage::MakeFromRaster(pixmap_u, release_proc, nullptr);
        image_v = SkImage::MakeFromRaster(pixmap_v, release_proc, nullptr);
      }
    } else {  // RGBX decoding is the default path.
      if (!DrawAndScaleImage(draw_image, &pixmap, generator_client_id_,
                             image_data->is_yuv)) {
        DLOG(ERROR) << "DrawAndScaleImage failed.";
        backing_memory->Unlock();
        backing_memory.reset();
      } else {
        image = SkImage::MakeFromRaster(pixmap, release_proc, nullptr);
      }
    }
  }

  if (image_data->decode.data()) {
    // An at-raster task decoded this before us. Ingore our decode.
    if (image_data->is_yuv) {
      DCHECK(image_data->decode.y_image());
      DCHECK(image_data->decode.u_image());
      DCHECK(image_data->decode.v_image());
    } else {
      DCHECK(image_data->decode.image());
    }
    return;
  }

  if (!backing_memory) {
    DCHECK(!image);
    DCHECK(!image_y);
    DCHECK(!image_u);
    DCHECK(!image_v);
    // If |backing_memory| was not populated, we had a non-decodable image.
    image_data->decode.decode_failure = true;
    return;
  }

  if (image_data->is_yuv) {
    image_data->decode.SetLockedData(
        std::move(backing_memory), std::move(image_y), std::move(image_u),
        std::move(image_v), task_type == TaskType::kOutOfRaster);
  } else {
    image_data->decode.SetLockedData(std::move(backing_memory),
                                     std::move(image),
                                     task_type == TaskType::kOutOfRaster);
  }
}

void GpuImageDecodeCache::UploadImageIfNecessary(const DrawImage& draw_image,
                                                 ImageData* image_data) {
  CheckContextLockAcquiredIfNecessary();
  lock_.AssertAcquired();

  // We are about to upload a new image and are holding the context lock.
  // Ensure that any images which have been marked for deletion are actually
  // cleaned up so we don't exceed our memory limit during this upload.
  RunPendingContextThreadOperations();

  if (image_data->decode.decode_failure) {
    // We were unable to decode this image. Don't try to upload.
    return;
  }

  // If an upload already exists, try to lock it. If this fails, it will clear
  // any uploaded data.
  if (image_data->HasUploadedData())
    TryLockImage(HaveContextLock::kYes, draw_image, image_data);

  // Ensure the mip status is correct before returning the locked upload or
  // preparing to upload a new image.
  UpdateMipsIfNeeded(draw_image, image_data);

  // If we have uploaded data at this point, it is locked with correct mips,
  // just return.
  if (image_data->HasUploadedData())
    return;

  TRACE_EVENT0("cc", "GpuImageDecodeCache::UploadImage");
  if (!image_data->decode.do_hardware_accelerated_decode()) {
    // These are not needed for accelerated decodes because there was no decode
    // task.
    DCHECK(image_data->decode.is_locked());
    image_data->decode.mark_used();
  }
  DCHECK_GT(image_data->decode.ref_count, 0u);
  DCHECK_GT(image_data->upload.ref_count, 0u);

  sk_sp<SkColorSpace> color_space =
      SupportsColorSpaceConversion() &&
              draw_image.target_color_space().IsValid()
          ? draw_image.target_color_space().ToSkColorSpace()
          : nullptr;
  // The value of |decoded_target_colorspace| takes into account the fact
  // that we might need to ignore an embedded image color space if |color_type_|
  // does not support color space conversions or that color conversion might
  // have happened at decode time.
  sk_sp<SkColorSpace> decoded_target_colorspace =
      ColorSpaceForImageDecode(draw_image, image_data->mode);
  if (color_space && SkColorSpace::Equals(color_space.get(),
                                          decoded_target_colorspace.get())) {
    color_space = nullptr;
  }

  if (image_data->mode == DecodedDataMode::kTransferCache) {
    DCHECK(use_transfer_cache_);
    if (image_data->decode.do_hardware_accelerated_decode()) {
      // The assumption is that scaling is not currently supported for
      // hardware-accelerated decodes.
      DCHECK_EQ(0, image_data->upload_scale_mip_level);
      const gfx::Size output_size(draw_image.paint_image().width(),
                                  draw_image.paint_image().height());

      // Get the encoded data in a contiguous form.
      sk_sp<SkData> encoded_data =
          draw_image.paint_image().GetSkImage()->refEncodedData();
      DCHECK(encoded_data);
      const uint32_t transfer_cache_id =
          ClientImageTransferCacheEntry::GetNextId();
      const gpu::SyncToken decode_sync_token =
          context_->RasterInterface()->ScheduleImageDecode(
              base::make_span(encoded_data->bytes(), encoded_data->size()),
              output_size, transfer_cache_id,
              color_space ? gfx::ColorSpace(*color_space) : gfx::ColorSpace(),
              image_data->needs_mips);

      if (!decode_sync_token.HasData()) {
        image_data->decode.decode_failure = true;
        return;
      }

      image_data->upload.SetTransferCacheId(transfer_cache_id);

      // Note that we wait for the decode sync token here for two reasons:
      //
      // 1) To make sure that raster work that depends on the image decode
      //    happens after the decode completes.
      //
      // 2) To protect the transfer cache entry from being unlocked on the
      //    service side before the decode is completed.
      context_->RasterInterface()->WaitSyncTokenCHROMIUM(
          decode_sync_token.GetConstData());

      return;
    }

    // Non-hardware-accelerated path.
    if (image_data->is_yuv) {
      DCHECK(image_data->yuv_color_space);
      SkPixmap y_pixmap;
      SkPixmap u_pixmap;
      SkPixmap v_pixmap;
      if (!image_data->decode.y_image()->peekPixels(&y_pixmap) ||
          !image_data->decode.u_image()->peekPixels(&u_pixmap) ||
          !image_data->decode.v_image()->peekPixels(&v_pixmap)) {
        return;
      }
      ClientImageTransferCacheEntry image_entry(
          &y_pixmap, &u_pixmap, &v_pixmap, decoded_target_colorspace.get(),
          image_data->yuv_color_space.value(), image_data->needs_mips);
      InsertTransferCacheEntry(image_entry, image_data);
    } else {
      SkPixmap pixmap;
      if (!image_data->decode.image()->peekPixels(&pixmap))
        return;
      ClientImageTransferCacheEntry image_entry(&pixmap, color_space.get(),
                                                image_data->needs_mips);
      InsertTransferCacheEntry(image_entry, image_data);
    }

    return;
  }

  // If we reached this point, we are in the CPU/GPU path (not transfer cache).
  DCHECK(!use_transfer_cache_);

  // Grab a reference to our decoded image. For the kCpu path, we will use
  // this directly as our "uploaded" data.
  sk_sp<SkImage> uploaded_image = image_data->decode.image();
  GrMipMapped image_needs_mips =
      image_data->needs_mips ? GrMipMapped::kYes : GrMipMapped::kNo;

  if (image_data->is_yuv) {
    DCHECK(image_data->yuv_color_space.has_value());
    // Grab a reference to our decoded image. For the kCpu path, we will use
    // this directly as our "uploaded" data.
    sk_sp<SkImage> uploaded_y_image = image_data->decode.y_image();
    sk_sp<SkImage> uploaded_u_image = image_data->decode.u_image();
    sk_sp<SkImage> uploaded_v_image = image_data->decode.v_image();

    // For kGpu, we upload and color convert (if necessary).
    if (image_data->mode == DecodedDataMode::kGpu) {
      DCHECK(!use_transfer_cache_);
      base::AutoUnlock unlock(lock_);
      uploaded_y_image = uploaded_y_image->makeTextureImage(
          context_->GrContext(), image_needs_mips);
      uploaded_u_image = uploaded_u_image->makeTextureImage(
          context_->GrContext(), image_needs_mips);
      uploaded_v_image = uploaded_v_image->makeTextureImage(
          context_->GrContext(), image_needs_mips);
      if (!uploaded_y_image || !uploaded_u_image || !uploaded_v_image) {
        DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
        return;
      }

      size_t image_width = uploaded_y_image->width();
      size_t image_height = uploaded_y_image->height();
      uploaded_image = CreateImageFromYUVATexturesInternal(
          uploaded_y_image.get(), uploaded_u_image.get(),
          uploaded_v_image.get(), image_width, image_height,
          image_data->yuv_color_space.value(), color_space,
          decoded_target_colorspace);
    }

    // At-raster may have decoded this while we were unlocked. If so, ignore our
    // result.
    if (image_data->HasUploadedData()) {
      if (uploaded_image) {
        DCHECK(uploaded_y_image);
        DCHECK(uploaded_u_image);
        DCHECK(uploaded_v_image);
        // We do not call DeleteSkImageAndPreventCaching for |uploaded_image|
        // because calls to getBackendTexture will flatten the YUV planes to
        // an RGB texture only to immediately delete it.
        DeleteSkImageAndPreventCaching(context_, std::move(uploaded_y_image));
        DeleteSkImageAndPreventCaching(context_, std::move(uploaded_u_image));
        DeleteSkImageAndPreventCaching(context_, std::move(uploaded_v_image));
      }
      return;
    }

    // TODO(crbug.com/740737): |uploaded_image| is sometimes null in certain
    // context-lost situations, so it is handled with an early out.
    if (!uploaded_image || !uploaded_y_image || !uploaded_u_image ||
        !uploaded_v_image) {
      DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
      return;
    }

    uploaded_y_image = TakeOwnershipOfSkImageBacking(
        context_->GrContext(), std::move(uploaded_y_image));
    uploaded_u_image = TakeOwnershipOfSkImageBacking(
        context_->GrContext(), std::move(uploaded_u_image));
    uploaded_v_image = TakeOwnershipOfSkImageBacking(
        context_->GrContext(), std::move(uploaded_v_image));

    image_data->upload.SetImage(std::move(uploaded_image), image_data->is_yuv);
    image_data->upload.SetYuvImage(std::move(uploaded_y_image),
                                   std::move(uploaded_u_image),
                                   std::move(uploaded_v_image));

    // If we have a new GPU-backed image, initialize it for use in the GPU
    // discardable system.
    if (image_data->mode == DecodedDataMode::kGpu) {
      // Notify the discardable system of the planes so they will count against
      // budgets.
      context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
          image_data->upload.gl_y_id());
      context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
          image_data->upload.gl_u_id());
      context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
          image_data->upload.gl_v_id());
    }
    // YUV decoding ends.
    return;
  }

  // RGBX decoding is below.
  // For kGpu, we upload and color convert (if necessary).
  if (image_data->mode == DecodedDataMode::kGpu) {
    DCHECK(!use_transfer_cache_);
    base::AutoUnlock unlock(lock_);
    uploaded_image = MakeTextureImage(context_, std::move(uploaded_image),
                                      color_space, image_needs_mips);
  }

  // At-raster may have decoded this while we were unlocked. If so, ignore our
  // result.
  if (image_data->upload.image()) {
    if (uploaded_image)
      DeleteSkImageAndPreventCaching(context_, std::move(uploaded_image));
    return;
  }

  // Take ownership of any GL texture backing for the SkImage. This allows
  // us to use the image with the discardable system.
  if (uploaded_image) {
    uploaded_image = TakeOwnershipOfSkImageBacking(context_->GrContext(),
                                                   std::move(uploaded_image));
  }

  // TODO(crbug.com/740737): uploaded_image is sometimes null in certain
  // context-lost situations.
  if (!uploaded_image) {
    DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
    return;
  }

  image_data->upload.SetImage(std::move(uploaded_image));

  // If we have a new GPU-backed image, initialize it for use in the GPU
  // discardable system.
  if (image_data->mode == DecodedDataMode::kGpu) {
    // Notify the discardable system of this image so it will count against
    // budgets.
    context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
        image_data->upload.gl_id());
  }
}

scoped_refptr<GpuImageDecodeCache::ImageData>
GpuImageDecodeCache::CreateImageData(const DrawImage& draw_image,
                                     bool allow_hardware_decode) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::CreateImageData");
  lock_.AssertAcquired();

  int upload_scale_mip_level = CalculateUploadScaleMipLevel(draw_image);
  bool needs_mips = ShouldGenerateMips(draw_image, upload_scale_mip_level);
  SkImageInfo image_info =
      CreateImageInfoForDrawImage(draw_image, upload_scale_mip_level);
  const bool image_larger_than_max_texture =
      image_info.width() > max_texture_size_ ||
      image_info.height() > max_texture_size_;
  DecodedDataMode mode;
  if (use_transfer_cache_) {
    mode = DecodedDataMode::kTransferCache;
  } else if (image_larger_than_max_texture) {
    // Image too large to upload. Try to use SW fallback.
    mode = DecodedDataMode::kCpu;
  } else {
    mode = DecodedDataMode::kGpu;
  }

  size_t data_size = image_info.computeMinByteSize();
  DCHECK(!SkImageInfo::ByteSizeOverflowed(data_size));

  // We need to cache the result of color conversion on the cpu if the image
  // will be color converted during the decode.
  auto decode_color_space = ColorSpaceForImageDecode(draw_image, mode);
  const bool cache_color_conversion_on_cpu =
      decode_color_space &&
      !SkColorSpace::Equals(decode_color_space.get(),
                            draw_image.paint_image().color_space());

  // |is_bitmap_backed| specifies whether the image has pixel data which can
  // directly be used for the upload. This will be the case for non-lazy images
  // used at the original scale. In these cases, we don't internally cache any
  // cpu component for the image.
  // However, if the image will be scaled or color converts on the cpu, we
  // consider it a lazy image and cache the scaled result in discardable memory.
  const bool is_bitmap_backed = !draw_image.paint_image().IsLazyGenerated() &&
                                upload_scale_mip_level == 0 &&
                                !cache_color_conversion_on_cpu;

  // Figure out if we will do hardware accelerated decoding. The criteria is as
  // follows:
  //
  // - The caller allows hardware decodes.
  // - We are using the transfer cache (OOP-R).
  // - The image does not require downscaling for uploading (see TODO below).
  // - The image does not require subsetting.
  // - The image is supported according to the profiles advertised by the GPU
  //   service.
  //
  // TODO(crbug.com/953367): currently, we don't support scaling with hardware
  // decode acceleration. Note that it's still okay for the image to be
  // downscaled by Skia using the GPU.
  const ImageHeaderMetadata* image_metadata =
      draw_image.paint_image().GetImageHeaderMetadata();
  bool can_do_hardware_accelerated_decode = false;
  bool do_hardware_accelerated_decode = false;
  if (allow_hardware_decode && mode == DecodedDataMode::kTransferCache &&
      upload_scale_mip_level == 0 &&
      draw_image.paint_image().subset_rect().IsEmpty() &&
      context_->ContextSupport()->CanDecodeWithHardwareAcceleration(
          image_metadata)) {
    DCHECK(image_metadata);
    DCHECK_EQ(image_metadata->image_size.width(),
              draw_image.paint_image().width());
    DCHECK_EQ(image_metadata->image_size.height(),
              draw_image.paint_image().height());

    can_do_hardware_accelerated_decode = true;
    const bool is_jpeg = (image_metadata->image_type == ImageType::kJPEG);
    const bool is_webp = (image_metadata->image_type == ImageType::kWEBP);
    if ((is_jpeg && allow_accelerated_jpeg_decodes_) ||
        (is_webp && allow_accelerated_webp_decodes_)) {
      do_hardware_accelerated_decode = true;
      data_size = EstimateHardwareDecodedDataSize(image_metadata);
      DCHECK(!is_bitmap_backed);
    }
  }

  SkYUVASizeInfo target_yuva_size_info;
  // We fill out a default value for |yuv_color_space| but only fill out the
  // base::Optional member in ImageData if it is YUV.
  SkYUVColorSpace yuv_color_space = SkYUVColorSpace::kIdentity_SkYUVColorSpace;
  const bool is_yuv =
      !do_hardware_accelerated_decode &&
      draw_image.paint_image().IsYuv(&target_yuva_size_info,
                                     nullptr /* plane_indices */,
                                     &yuv_color_space) &&
      mode != DecodedDataMode::kCpu && !image_larger_than_max_texture;
  // TODO(crbug.com/910276): Change after alpha support.
  if (is_yuv) {
    const gfx::Size target_raster_size(image_info.width(), image_info.height());
    gfx::Size unscaled_u_size(
        target_yuva_size_info.fSizes[SkYUVAIndex::kU_Index].width(),
        target_yuva_size_info.fSizes[SkYUVAIndex::kU_Index].height());
    gfx::Size unscaled_v_size(
        target_yuva_size_info.fSizes[SkYUVAIndex::kV_Index].width(),
        target_yuva_size_info.fSizes[SkYUVAIndex::kV_Index].height());
    gfx::Size scaled_u_size;
    gfx::Size scaled_v_size;
    ComputeMippedUVPlaneSizes(target_raster_size, unscaled_u_size,
                              unscaled_v_size, draw_image, &scaled_u_size,
                              &scaled_v_size);

    size_t y_size_bytes =
        target_yuva_size_info.fWidthBytes[SkYUVAIndex::kY_Index] *
        target_yuva_size_info.fSizes[SkYUVAIndex::kY_Index].height();
    size_t u_size_bytes =
        target_yuva_size_info.fWidthBytes[SkYUVAIndex::kU_Index] *
        target_yuva_size_info.fSizes[SkYUVAIndex::kU_Index].height();
    size_t v_size_bytes =
        target_yuva_size_info.fWidthBytes[SkYUVAIndex::kV_Index] *
        target_yuva_size_info.fSizes[SkYUVAIndex::kV_Index].height();
    data_size = y_size_bytes + u_size_bytes + v_size_bytes;
  }

  return base::WrapRefCounted(new ImageData(
      draw_image.paint_image().stable_id(), mode, data_size,
      draw_image.target_color_space(),
      CalculateDesiredFilterQuality(draw_image), upload_scale_mip_level,
      needs_mips, is_bitmap_backed, can_do_hardware_accelerated_decode,
      do_hardware_accelerated_decode, is_yuv, yuv_color_space));
}

void GpuImageDecodeCache::WillAddCacheEntry(const DrawImage& draw_image) {
  // Remove any old entries for this image. We keep at-most 2 ContentIds for a
  // PaintImage (pending and active tree).
  auto& cache_entries =
      paint_image_entries_[draw_image.paint_image().stable_id()];
  cache_entries.count++;

  auto& cached_content_ids = cache_entries.content_ids;
  const PaintImage::ContentId new_content_id =
      draw_image.frame_key().content_id();

  if (cached_content_ids[0] == new_content_id ||
      cached_content_ids[1] == new_content_id) {
    return;
  }

  if (cached_content_ids[0] == PaintImage::kInvalidContentId) {
    cached_content_ids[0] = new_content_id;
    return;
  }

  if (cached_content_ids[1] == PaintImage::kInvalidContentId) {
    cached_content_ids[1] = new_content_id;
    return;
  }

  const PaintImage::ContentId content_id_to_remove =
      std::min(cached_content_ids[0], cached_content_ids[1]);
  const PaintImage::ContentId content_id_to_keep =
      std::max(cached_content_ids[0], cached_content_ids[1]);
  DCHECK_NE(content_id_to_remove, content_id_to_keep);

  for (auto it = persistent_cache_.begin(); it != persistent_cache_.end();) {
    if (it->first.content_id() != content_id_to_remove) {
      ++it;
    } else {
      it = RemoveFromPersistentCache(it);
    }
  }

  // Removing entries from the persistent cache should not erase the tracking
  // for the current paint_image, since we have 2 different content ids for it
  // and only one of them was erased above.
  DCHECK_NE(paint_image_entries_.count(draw_image.paint_image().stable_id()),
            0u);

  cached_content_ids[0] = content_id_to_keep;
  cached_content_ids[1] = new_content_id;
}

void GpuImageDecodeCache::DeleteImage(ImageData* image_data) {
  if (image_data->HasUploadedData()) {
    DCHECK(!image_data->upload.is_locked());
    if (image_data->mode == DecodedDataMode::kGpu) {
      if (image_data->is_yuv) {
        images_pending_deletion_.push_back(image_data->upload.y_image());
        images_pending_deletion_.push_back(image_data->upload.u_image());
        images_pending_deletion_.push_back(image_data->upload.v_image());
        yuv_images_pending_deletion_.push_back(image_data->upload.image());
      } else {
        images_pending_deletion_.push_back(image_data->upload.image());
      }
    }
    if (image_data->mode == DecodedDataMode::kTransferCache)
      ids_pending_deletion_.push_back(*image_data->upload.transfer_cache_id());
  }
  image_data->upload.Reset();
}

void GpuImageDecodeCache::UnlockImage(ImageData* image_data) {
  DCHECK(image_data->HasUploadedData());
  if (image_data->mode == DecodedDataMode::kGpu) {
    if (image_data->is_yuv) {
      images_pending_unlock_.push_back(image_data->upload.y_image().get());
      images_pending_unlock_.push_back(image_data->upload.u_image().get());
      images_pending_unlock_.push_back(image_data->upload.v_image().get());
      yuv_images_pending_unlock_.push_back(image_data->upload.image());
    } else {
      images_pending_unlock_.push_back(image_data->upload.image().get());
    }
  } else {
    DCHECK(image_data->mode == DecodedDataMode::kTransferCache);
    ids_pending_unlock_.push_back(*image_data->upload.transfer_cache_id());
  }
  image_data->upload.OnUnlock();

  // If we were holding onto an unmipped image for defering deletion, do it now
  // it is guarenteed to have no-refs.
  auto unmipped_image = image_data->upload.take_unmipped_image();
  if (unmipped_image) {
    if (image_data->is_yuv) {
      auto unmipped_y_image = image_data->upload.take_unmipped_y_image();
      auto unmipped_u_image = image_data->upload.take_unmipped_u_image();
      auto unmipped_v_image = image_data->upload.take_unmipped_v_image();
      DCHECK(unmipped_y_image);
      DCHECK(unmipped_u_image);
      DCHECK(unmipped_v_image);
      images_pending_deletion_.push_back(std::move(unmipped_y_image));
      images_pending_deletion_.push_back(std::move(unmipped_u_image));
      images_pending_deletion_.push_back(std::move(unmipped_v_image));
      yuv_images_pending_deletion_.push_back(std::move(unmipped_image));
    } else {
      images_pending_deletion_.push_back(std::move(unmipped_image));
    }
  }
}

// YUV images are handled slightly differently because they are not themselves
// registered with the discardable memory system. We cannot use
// GlIdFromSkImage on these YUV SkImages to flush pending operations because
// doing so will flatten it to RGB.
void GpuImageDecodeCache::FlushYUVImages(
    std::vector<sk_sp<SkImage>>* yuv_images) {
  CheckContextLockAcquiredIfNecessary();
  lock_.AssertAcquired();
  for (auto& image : *yuv_images) {
    image->flush(context_->GrContext());
  }
  yuv_images->clear();
}

// We always run pending operations in the following order:
//   > Lock
//   > Flush YUV images that will be unlocked
//   > Unlock
//   > Flush YUV images that will be deleted
//   > Delete
// This ensures that:
//   a) We never fully unlock an image that's pending lock (lock before unlock)
//   b) We never delete an image that has pending locks/unlocks.
//   c) We never unlock or delete the underlying texture planes for a YUV
//      image before all operations referencing it have completed.
//
// As this can be run at-raster, to unlock/delete an image that was just used,
// we need to call GlIdFromSkImage, which flushes pending IO on the image,
// rather than just using a cached GL ID.
// YUV images are handled slightly differently because they are backed by
// texture images but are not themselves registered with the discardable memory
// system. We wait to delete the pointer to a YUV image until we have a context
// lock and its textures have been deleted.
void GpuImageDecodeCache::RunPendingContextThreadOperations() {
  CheckContextLockAcquiredIfNecessary();
  lock_.AssertAcquired();

  for (auto* image : images_pending_complete_lock_) {
    context_->ContextSupport()->CompleteLockDiscardableTexureOnContextThread(
        GlIdFromSkImage(image));
  }
  images_pending_complete_lock_.clear();

  FlushYUVImages(&yuv_images_pending_unlock_);
  for (auto* image : images_pending_unlock_) {
    context_->RasterInterface()->UnlockDiscardableTextureCHROMIUM(
        GlIdFromSkImage(image));
  }
  images_pending_unlock_.clear();

  for (auto id : ids_pending_unlock_) {
    context_->ContextSupport()->UnlockTransferCacheEntries({std::make_pair(
        static_cast<uint32_t>(TransferCacheEntryType::kImage), id)});
  }
  ids_pending_unlock_.clear();

  FlushYUVImages(&yuv_images_pending_deletion_);
  for (auto& image : images_pending_deletion_) {
    uint32_t texture_id = GlIdFromSkImage(image.get());
    if (context_->RasterInterface()->LockDiscardableTextureCHROMIUM(
            texture_id)) {
      context_->RasterInterface()->DeleteGpuRasterTexture(texture_id);
    }
  }
  images_pending_deletion_.clear();

  for (auto id : ids_pending_deletion_) {
    if (context_->ContextSupport()->ThreadsafeLockTransferCacheEntry(
            static_cast<uint32_t>(TransferCacheEntryType::kImage), id)) {
      context_->ContextSupport()->DeleteTransferCacheEntry(
          static_cast<uint32_t>(TransferCacheEntryType::kImage), id);
    }
  }
  ids_pending_deletion_.clear();
}

SkImageInfo GpuImageDecodeCache::CreateImageInfoForDrawImage(
    const DrawImage& draw_image,
    int upload_scale_mip_level) const {
  gfx::Size mip_size =
      CalculateSizeForMipLevel(draw_image, upload_scale_mip_level);
  return SkImageInfo::Make(mip_size.width(), mip_size.height(), color_type_,
                           kPremul_SkAlphaType);
}

bool GpuImageDecodeCache::TryLockImage(HaveContextLock have_context_lock,
                                       const DrawImage& draw_image,
                                       ImageData* data) {
  DCHECK(data->HasUploadedData());

  if (data->upload.is_locked())
    return true;

  if (data->mode == DecodedDataMode::kTransferCache) {
    DCHECK(use_transfer_cache_);
    DCHECK(data->upload.transfer_cache_id());
    if (context_->ContextSupport()->ThreadsafeLockTransferCacheEntry(
            static_cast<uint32_t>(TransferCacheEntryType::kImage),
            *data->upload.transfer_cache_id())) {
      data->upload.OnLock();
      return true;
    }
  } else if (have_context_lock == HaveContextLock::kYes) {
    auto* ri = context_->RasterInterface();
    // If |have_context_lock|, we can immediately lock the image and send
    // the lock command to the GPU process.
    // TODO(crbug.com/914622): Add Chrome GL extension to upload texture array.
    if (data->is_yuv &&
        ri->LockDiscardableTextureCHROMIUM(data->upload.gl_y_id()) &&
        ri->LockDiscardableTextureCHROMIUM(data->upload.gl_u_id()) &&
        ri->LockDiscardableTextureCHROMIUM(data->upload.gl_v_id())) {
      DCHECK(!use_transfer_cache_);
      DCHECK(data->mode == DecodedDataMode::kGpu);
      data->upload.OnLock();
      return true;
    } else if (!(data->is_yuv) &&
               ri->LockDiscardableTextureCHROMIUM(data->upload.gl_id())) {
      DCHECK(!use_transfer_cache_);
      DCHECK(data->mode == DecodedDataMode::kGpu);
      data->upload.OnLock();
      return true;
    }
  } else {
    // If !|have_context_lock|, we use
    // ThreadsafeShallowLockDiscardableTexture. This takes a reference to the
    // image, ensuring that it can't be deleted by the service, but delays
    // sending a lock command over the command buffer. This command must be
    // sent before the image is used, but is now guaranteed to succeed. We
    // will send this command via
    // CompleteLockDiscardableTextureOnContextThread in
    // UploadImageIfNecessary, which is guaranteed to run before the texture
    // is used.
    auto* context_support = context_->ContextSupport();
    if (data->is_yuv &&
        context_support->ThreadSafeShallowLockDiscardableTexture(
            data->upload.gl_y_id()) &&
        context_support->ThreadSafeShallowLockDiscardableTexture(
            data->upload.gl_u_id()) &&
        context_support->ThreadSafeShallowLockDiscardableTexture(
            data->upload.gl_v_id())) {
      DCHECK(!use_transfer_cache_);
      DCHECK(data->mode == DecodedDataMode::kGpu);
      data->upload.OnLock();
      images_pending_complete_lock_.push_back(data->upload.y_image().get());
      images_pending_complete_lock_.push_back(data->upload.u_image().get());
      images_pending_complete_lock_.push_back(data->upload.v_image().get());
      return true;
    } else if (!(data->is_yuv) &&
               context_support->ThreadSafeShallowLockDiscardableTexture(
                   data->upload.gl_id())) {
      DCHECK(!use_transfer_cache_);
      DCHECK(data->mode == DecodedDataMode::kGpu);
      data->upload.OnLock();
      images_pending_complete_lock_.push_back(data->upload.image().get());
      return true;
    }
  }

  // Couldn't lock, abandon the image.
  DeleteImage(data);
  return false;
}

// Tries to find an ImageData that can be used to draw the provided
// |draw_image|. First looks for an exact entry in our |in_use_cache_|. If one
// cannot be found, it looks for a compatible entry in our |persistent_cache_|.
GpuImageDecodeCache::ImageData* GpuImageDecodeCache::GetImageDataForDrawImage(
    const DrawImage& draw_image,
    const InUseCacheKey& key) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDataForDrawImage");
  lock_.AssertAcquired();
  DCHECK(UseCacheForDrawImage(draw_image));

  auto found_in_use = in_use_cache_.find(key);
  if (found_in_use != in_use_cache_.end())
    return found_in_use->second.image_data.get();

  auto found_persistent = persistent_cache_.Get(draw_image.frame_key());
  if (found_persistent != persistent_cache_.end()) {
    ImageData* image_data = found_persistent->second.get();
    if (IsCompatible(image_data, draw_image)) {
      return image_data;
    } else {
      RemoveFromPersistentCache(found_persistent);
    }
  }

  return nullptr;
}

// Determines if we can draw the provided |draw_image| using the provided
// |image_data|. This is true if the |image_data| is not scaled, or if it
// is scaled at an equal or larger scale and equal or larger quality to
// the provided |draw_image|.
bool GpuImageDecodeCache::IsCompatible(const ImageData* image_data,
                                       const DrawImage& draw_image) const {
  bool is_scaled = image_data->upload_scale_mip_level != 0;
  bool scale_is_compatible = CalculateUploadScaleMipLevel(draw_image) >=
                             image_data->upload_scale_mip_level;
  bool quality_is_compatible =
      CalculateDesiredFilterQuality(draw_image) <= image_data->quality;
  bool color_is_compatible =
      image_data->target_color_space == draw_image.target_color_space();
  if (!color_is_compatible)
    return false;
  if (is_scaled && (!scale_is_compatible || !quality_is_compatible))
    return false;
  return true;
}

size_t GpuImageDecodeCache::GetDrawImageSizeForTesting(const DrawImage& image) {
  base::AutoLock lock(lock_);
  scoped_refptr<ImageData> data =
      CreateImageData(image, false /* allow_hardware_decode */);
  return data->size;
}

void GpuImageDecodeCache::SetImageDecodingFailedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  image_data->decode.decode_failure = true;
}

bool GpuImageDecodeCache::DiscardableIsLockedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  return image_data->decode.is_locked();
}

bool GpuImageDecodeCache::IsInInUseCacheForTesting(
    const DrawImage& image) const {
  auto found = in_use_cache_.find(InUseCacheKey::FromDrawImage(image));
  return found != in_use_cache_.end();
}

bool GpuImageDecodeCache::IsInPersistentCacheForTesting(
    const DrawImage& image) const {
  auto found = persistent_cache_.Peek(image.frame_key());
  return found != persistent_cache_.end();
}

sk_sp<SkImage> GpuImageDecodeCache::GetSWImageDecodeForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.frame_key());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  DCHECK(!image_data->is_yuv);
  return image_data->decode.ImageForTesting();
}

// Used for in-process-raster YUV decoding tests, where we often need the
// SkImages for each underlying plane because asserting or requesting fields for
// the YUV SkImage may flatten it to RGB or not be possible to request.
sk_sp<SkImage> GpuImageDecodeCache::GetUploadedPlaneForTesting(
    const DrawImage& draw_image,
    size_t index) {
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(
      draw_image, InUseCacheKey::FromDrawImage(draw_image));
  switch (index) {
    case SkYUVAIndex::kY_Index:
      return image_data->upload.y_image();
    case SkYUVAIndex::kU_Index:
      return image_data->upload.u_image();
    case SkYUVAIndex::kV_Index:
      return image_data->upload.v_image();
    default:
      return nullptr;
  }
}

void GpuImageDecodeCache::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  base::AutoLock lock(lock_);
  switch (level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      base::AutoReset<bool> reset(&aggressively_freeing_resources_, true);
      EnsureCapacity(0);
      break;
  }
}

bool GpuImageDecodeCache::SupportsColorSpaceConversion() const {
  switch (color_type_) {
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
    case kRGBA_F16_SkColorType:
      return true;
    default:
      return false;
  }
}

sk_sp<SkColorSpace> GpuImageDecodeCache::ColorSpaceForImageDecode(
    const DrawImage& image,
    DecodedDataMode mode) const {
  if (!SupportsColorSpaceConversion())
    return nullptr;

  if (mode == DecodedDataMode::kCpu)
    return image.target_color_space().ToSkColorSpace();

  // For kGpu or kTransferCache images color conversion is handled during
  // upload, so keep the original colorspace here.
  return sk_ref_sp(image.paint_image().color_space());
}

void GpuImageDecodeCache::CheckContextLockAcquiredIfNecessary() {
  if (!context_->GetLock())
    return;
  context_->GetLock()->AssertAcquired();
}

sk_sp<SkImage> GpuImageDecodeCache::CreateImageFromYUVATexturesInternal(
    const SkImage* uploaded_y_image,
    const SkImage* uploaded_u_image,
    const SkImage* uploaded_v_image,
    const size_t image_width,
    const size_t image_height,
    const SkYUVColorSpace& yuv_color_space,
    sk_sp<SkColorSpace> target_color_space,
    sk_sp<SkColorSpace> decoded_color_space) const {
  DCHECK(uploaded_y_image);
  DCHECK(uploaded_u_image);
  DCHECK(uploaded_v_image);
  GrSurfaceOrigin origin_temp = kTopLeft_GrSurfaceOrigin;
  GrBackendTexture yuv_textures[3]{};
  yuv_textures[0] = uploaded_y_image->getBackendTexture(false);
  yuv_textures[1] = uploaded_u_image->getBackendTexture(false);
  yuv_textures[2] = uploaded_v_image->getBackendTexture(false);

  SkYUVAIndex indices[SkYUVAIndex::kIndexCount];
  indices[SkYUVAIndex::kY_Index] = {0, SkColorChannel::kR};
  indices[SkYUVAIndex::kU_Index] = {1, SkColorChannel::kR};
  indices[SkYUVAIndex::kV_Index] = {2, SkColorChannel::kR};
  indices[SkYUVAIndex::kA_Index] = {-1, SkColorChannel::kR};

  if (target_color_space && SkColorSpace::Equals(target_color_space.get(),
                                                 decoded_color_space.get())) {
    target_color_space = nullptr;
  }

  sk_sp<SkImage> yuva_image = SkImage::MakeFromYUVATextures(
      context_->GrContext(), yuv_color_space, yuv_textures, indices,
      SkISize::Make(image_width, image_height), origin_temp,
      std::move(decoded_color_space));
  if (target_color_space)
    return yuva_image->makeColorSpace(target_color_space);

  return yuva_image;
}

void GpuImageDecodeCache::UpdateMipsIfNeeded(const DrawImage& draw_image,
                                             ImageData* image_data) {
  CheckContextLockAcquiredIfNecessary();
  // If we already have mips, nothing to do.
  if (image_data->needs_mips)
    return;

  bool needs_mips =
      ShouldGenerateMips(draw_image, image_data->upload_scale_mip_level);
  if (!needs_mips)
    return;

  image_data->needs_mips = true;

  // If we have no uploaded image, nothing to do other than update needs_mips.
  // Mips will be generated during later upload.
  if (!image_data->HasUploadedData() ||
      image_data->mode != DecodedDataMode::kGpu)
    return;

  if (image_data->is_yuv) {
    // Need to generate mips. Take a reference on the planes we're about to
    // delete, delaying deletion.
    // TODO(crbug.com/910276): Change after alpha support.
    sk_sp<SkImage> previous_y_image = image_data->upload.y_image();
    sk_sp<SkImage> previous_u_image = image_data->upload.u_image();
    sk_sp<SkImage> previous_v_image = image_data->upload.v_image();

    // Generate a new image from the previous, adding mips.
    sk_sp<SkImage> image_y_with_mips = previous_y_image->makeTextureImage(
        context_->GrContext(), GrMipMapped::kYes);
    sk_sp<SkImage> image_u_with_mips = previous_u_image->makeTextureImage(
        context_->GrContext(), GrMipMapped::kYes);
    sk_sp<SkImage> image_v_with_mips = previous_v_image->makeTextureImage(
        context_->GrContext(), GrMipMapped::kYes);

    // Handle lost context.
    if (!image_y_with_mips || !image_u_with_mips || !image_v_with_mips) {
      DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
      return;
    }

    // No need to do anything if mipping this image results in the same
    // textures. Deleting it below will result in lifetime issues.
    // We expect that if one plane mips the same, the others should as well.
    if (GlIdFromSkImage(image_y_with_mips.get()) ==
            image_data->upload.gl_y_id() &&
        GlIdFromSkImage(image_u_with_mips.get()) ==
            image_data->upload.gl_u_id() &&
        GlIdFromSkImage(image_v_with_mips.get()) ==
            image_data->upload.gl_v_id())
      return;

    // Skia owns our new image planes, take ownership.
    sk_sp<SkImage> image_y_with_mips_owned = TakeOwnershipOfSkImageBacking(
        context_->GrContext(), std::move(image_y_with_mips));
    sk_sp<SkImage> image_u_with_mips_owned = TakeOwnershipOfSkImageBacking(
        context_->GrContext(), std::move(image_u_with_mips));
    sk_sp<SkImage> image_v_with_mips_owned = TakeOwnershipOfSkImageBacking(
        context_->GrContext(), std::move(image_v_with_mips));

    // Handle lost context
    if (!image_y_with_mips_owned || !image_u_with_mips_owned ||
        !image_v_with_mips_owned) {
      DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
      return;
    }

    size_t width = image_y_with_mips_owned->width();
    size_t height = image_y_with_mips_owned->height();
    sk_sp<SkColorSpace> color_space =
        SupportsColorSpaceConversion() &&
                draw_image.target_color_space().IsValid()
            ? draw_image.target_color_space().ToSkColorSpace()
            : nullptr;
    sk_sp<SkColorSpace> decoded_color_space =
        ColorSpaceForImageDecode(draw_image, image_data->mode);
    DCHECK(image_data->yuv_color_space.has_value());
    sk_sp<SkImage> yuv_image_with_mips_owned =
        CreateImageFromYUVATexturesInternal(
            image_y_with_mips_owned.get(), image_u_with_mips_owned.get(),
            image_v_with_mips_owned.get(), width, height,
            image_data->yuv_color_space.value(), color_space,
            decoded_color_space);
    // In case of lost context
    if (!yuv_image_with_mips_owned) {
      DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
      return;
    }

    // The previous images might be in the in-use cache, potentially held
    // externally. We must defer deleting them until the entry is unlocked.
    image_data->upload.set_unmipped_image(image_data->upload.image());
    image_data->upload.set_unmipped_yuv_images(image_data->upload.y_image(),
                                               image_data->upload.u_image(),
                                               image_data->upload.v_image());

    // Set the new image on the cache.
    image_data->upload.Reset();
    image_data->upload.SetImage(std::move(yuv_image_with_mips_owned));
    image_data->upload.SetYuvImage(std::move(image_y_with_mips_owned),
                                   std::move(image_u_with_mips_owned),
                                   std::move(image_v_with_mips_owned));
    context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
        image_data->upload.gl_y_id());
    context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
        image_data->upload.gl_u_id());
    context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
        image_data->upload.gl_v_id());
    return;  // End YUV mip mapping.
  }
  // Begin RGBX mip mapping.
  // Need to generate mips. Take a reference on the image we're about to
  // delete, delaying deletion.
  sk_sp<SkImage> previous_image = image_data->upload.image();

  // Generate a new image from the previous, adding mips.
  sk_sp<SkImage> image_with_mips = previous_image->makeTextureImage(
      context_->GrContext(), GrMipMapped::kYes);

  // Handle lost context.
  if (!image_with_mips) {
    DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
    return;
  }

  // No need to do anything if mipping this image results in the same texture.
  // Deleting it below will result in lifetime issues.
  if (GlIdFromSkImage(image_with_mips.get()) == image_data->upload.gl_id())
    return;

  // Skia owns our new image, take ownership.
  sk_sp<SkImage> image_with_mips_owned = TakeOwnershipOfSkImageBacking(
      context_->GrContext(), std::move(image_with_mips));

  // Handle lost context
  if (!image_with_mips_owned) {
    DLOG(WARNING) << "TODO(crbug.com/740737): Context was lost. Early out.";
    return;
  }

  // The previous image might be in the in-use cache, potentially held
  // externally. We must defer deleting it until the entry is unlocked.
  image_data->upload.set_unmipped_image(image_data->upload.image());

  // Set the new image on the cache.
  image_data->upload.Reset();
  image_data->upload.SetImage(std::move(image_with_mips_owned));
  context_->RasterInterface()->InitializeDiscardableTextureCHROMIUM(
      image_data->upload.gl_id());
}

}  // namespace cc
