/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/graphics/ImageBuffer.h"

#include <memory>
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageBufferClient.h"
#include "platform/graphics/RecordingImageBufferSurface.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-encoders/JPEGImageEncoder.h"
#include "platform/image-encoders/PNGImageEncoder.h"
#include "platform/image-encoders/WEBPImageEncoder.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace blink {

std::unique_ptr<ImageBuffer> ImageBuffer::Create(
    std::unique_ptr<ImageBufferSurface> surface) {
  if (!surface->IsValid())
    return nullptr;
  return WTF::WrapUnique(new ImageBuffer(std::move(surface)));
}

std::unique_ptr<ImageBuffer> ImageBuffer::Create(
    const IntSize& size,
    OpacityMode opacity_mode,
    ImageInitializationMode initialization_mode,
    sk_sp<SkColorSpace> color_space) {
  SkColorType color_type = kN32_SkColorType;
  if (color_space && SkColorSpace::Equals(color_space.get(),
                                          SkColorSpace::MakeSRGBLinear().get()))
    color_type = kRGBA_F16_SkColorType;

  std::unique_ptr<ImageBufferSurface> surface(
      WTF::WrapUnique(new UnacceleratedImageBufferSurface(
          size, opacity_mode, initialization_mode, std::move(color_space),
          color_type)));

  if (!surface->IsValid())
    return nullptr;
  return WTF::WrapUnique(new ImageBuffer(std::move(surface)));
}

ImageBuffer::ImageBuffer(std::unique_ptr<ImageBufferSurface> surface)
    : weak_ptr_factory_(this),
      snapshot_state_(kInitialSnapshotState),
      surface_(std::move(surface)),
      client_(0),
      gpu_readback_invoked_in_current_frame_(false),
      gpu_readback_successive_frames_(0),
      gpu_memory_usage_(0) {
  surface_->SetImageBuffer(this);
  UpdateGPUMemoryUsage();
}

intptr_t ImageBuffer::global_gpu_memory_usage_ = 0;
unsigned ImageBuffer::global_accelerated_image_buffer_count_ = 0;

ImageBuffer::~ImageBuffer() {
  if (gpu_memory_usage_) {
    DCHECK_GT(global_accelerated_image_buffer_count_, 0u);
    global_accelerated_image_buffer_count_--;
  }
  ImageBuffer::global_gpu_memory_usage_ -= gpu_memory_usage_;
  surface_->SetImageBuffer(nullptr);
}

bool ImageBuffer::CanCreateImageBuffer(const IntSize& size) {
  if (size.IsEmpty())
    return false;
  CheckedNumeric<int> area = size.Width();
  area *= size.Height();
  if (!area.IsValid() || area.ValueOrDie() > kMaxCanvasArea)
    return false;
  if (size.Width() > kMaxSkiaDim || size.Height() > kMaxSkiaDim)
    return false;
  return true;
}

PaintCanvas* ImageBuffer::Canvas() const {
  return surface_->Canvas();
}

void ImageBuffer::DisableDeferral(DisableDeferralReason reason) const {
  return surface_->DisableDeferral(reason);
}

bool ImageBuffer::WritePixels(const SkImageInfo& info,
                              const void* pixels,
                              size_t row_bytes,
                              int x,
                              int y) {
  return surface_->WritePixels(info, pixels, row_bytes, x, y);
}

bool ImageBuffer::IsSurfaceValid() const {
  return surface_->IsValid();
}

void ImageBuffer::FinalizeFrame() {
  if (IsAccelerated() &&
      ExpensiveCanvasHeuristicParameters::kGPUReadbackForcesNoAcceleration &&
      !RuntimeEnabledFeatures::canvas2dFixedRenderingModeEnabled()) {
    if (gpu_readback_invoked_in_current_frame_) {
      gpu_readback_successive_frames_++;
      gpu_readback_invoked_in_current_frame_ = false;
    } else {
      gpu_readback_successive_frames_ = 0;
    }

    if (gpu_readback_successive_frames_ >=
        ExpensiveCanvasHeuristicParameters::kGPUReadbackMinSuccessiveFrames) {
      DisableAcceleration();
    }
  }

  surface_->FinalizeFrame();
}

void ImageBuffer::DoPaintInvalidation(const FloatRect& dirty_rect) {
  surface_->DoPaintInvalidation(dirty_rect);
}

bool ImageBuffer::RestoreSurface() const {
  return surface_->IsValid() || surface_->Restore();
}

void ImageBuffer::NotifySurfaceInvalid() {
  if (client_)
    client_->NotifySurfaceInvalid();
}

void ImageBuffer::ResetCanvas(PaintCanvas* canvas) const {
  if (client_)
    client_->RestoreCanvasMatrixClipStack(canvas);
}

sk_sp<SkImage> ImageBuffer::NewSkImageSnapshot(AccelerationHint hint,
                                               SnapshotReason reason) const {
  if (snapshot_state_ == kInitialSnapshotState)
    snapshot_state_ = kDidAcquireSnapshot;

  if (!IsSurfaceValid())
    return nullptr;
  return surface_->NewImageSnapshot(hint, reason);
}

PassRefPtr<Image> ImageBuffer::NewImageSnapshot(AccelerationHint hint,
                                                SnapshotReason reason) const {
  sk_sp<SkImage> snapshot = NewSkImageSnapshot(hint, reason);
  if (!snapshot)
    return nullptr;
  return StaticBitmapImage::Create(std::move(snapshot));
}

void ImageBuffer::DidDraw(const FloatRect& rect) const {
  if (snapshot_state_ == kDidAcquireSnapshot)
    snapshot_state_ = kDrawnToAfterSnapshot;
  surface_->DidDraw(rect);
}

WebLayer* ImageBuffer::PlatformLayer() const {
  return surface_->Layer();
}

bool ImageBuffer::CopyToPlatformTexture(SnapshotReason reason,
                                        gpu::gles2::GLES2Interface* gl,
                                        GLenum target,
                                        GLuint texture,
                                        bool premultiply_alpha,
                                        bool flip_y,
                                        const IntPoint& dest_point,
                                        const IntRect& source_sub_rectangle) {
  if (!Extensions3DUtil::CanUseCopyTextureCHROMIUM(target))
    return false;

  if (!IsSurfaceValid())
    return false;

  sk_sp<const SkImage> texture_image =
      surface_->NewImageSnapshot(kPreferAcceleration, reason);
  if (!texture_image)
    return false;

  if (!surface_->IsAccelerated())
    return false;

  DCHECK(texture_image->isTextureBacked());  // The isAccelerated() check above
                                             // should guarantee this.
  // Get the texture ID, flushing pending operations if needed.
  const GrGLTextureInfo* texture_info = skia::GrBackendObjectToGrGLTextureInfo(
      texture_image->getTextureHandle(true));
  if (!texture_info || !texture_info->fID)
    return false;

  std::unique_ptr<WebGraphicsContext3DProvider> provider = WTF::WrapUnique(
      Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider());
  if (!provider || !provider->GetGrContext())
    return false;
  gpu::gles2::GLES2Interface* shared_gl = provider->ContextGL();

  gpu::Mailbox mailbox;

  // Contexts may be in a different share group. We must transfer the texture
  // through a mailbox first.
  shared_gl->GenMailboxCHROMIUM(mailbox.name);
  shared_gl->ProduceTextureDirectCHROMIUM(texture_info->fID,
                                          texture_info->fTarget, mailbox.name);
  const GLuint64 shared_fence_sync = shared_gl->InsertFenceSyncCHROMIUM();
  shared_gl->Flush();

  gpu::SyncToken produce_sync_token;
  shared_gl->GenSyncTokenCHROMIUM(shared_fence_sync,
                                  produce_sync_token.GetData());
  gl->WaitSyncTokenCHROMIUM(produce_sync_token.GetConstData());

  GLuint source_texture =
      gl->CreateAndConsumeTextureCHROMIUM(texture_info->fTarget, mailbox.name);

  // The canvas is stored in a premultiplied format, so unpremultiply if
  // necessary. The canvas is also stored in an inverted position, so the flip
  // semantics are reversed.
  // It is expected that callers of this method have already allocated
  // the platform texture with the appropriate size.
  gl->CopySubTextureCHROMIUM(
      source_texture, 0, target, texture, 0, dest_point.X(), dest_point.Y(),
      source_sub_rectangle.X(), source_sub_rectangle.Y(),
      source_sub_rectangle.Width(), source_sub_rectangle.Height(),
      flip_y ? GL_FALSE : GL_TRUE, GL_FALSE,
      premultiply_alpha ? GL_FALSE : GL_TRUE);

  gl->DeleteTextures(1, &source_texture);

  const GLuint64 context_fence_sync = gl->InsertFenceSyncCHROMIUM();

  gl->Flush();

  gpu::SyncToken copy_sync_token;
  gl->GenSyncTokenCHROMIUM(context_fence_sync, copy_sync_token.GetData());
  shared_gl->WaitSyncTokenCHROMIUM(copy_sync_token.GetConstData());
  // This disassociates the texture from the mailbox to avoid leaking the
  // mapping between the two.
  shared_gl->ProduceTextureDirectCHROMIUM(0, texture_info->fTarget,
                                          mailbox.name);

  // Undo grContext texture binding changes introduced in this function.
  GrContext* gr_context = provider->GetGrContext();
  CHECK(gr_context);  // We already check / early-out above if null.
  gr_context->resetContext(kTextureBinding_GrGLBackendState);

  return true;
}

bool ImageBuffer::CopyRenderingResultsFromDrawingBuffer(
    DrawingBuffer* drawing_buffer,
    SourceDrawingBuffer source_buffer) {
  if (!drawing_buffer || !surface_->IsAccelerated())
    return false;
  std::unique_ptr<WebGraphicsContext3DProvider> provider = WTF::WrapUnique(
      Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider());
  if (!provider)
    return false;
  gpu::gles2::GLES2Interface* gl = provider->ContextGL();
  GLuint texture_id = surface_->GetBackingTextureHandleForOverwrite();
  if (!texture_id)
    return false;

  gl->Flush();

  return drawing_buffer->CopyToPlatformTexture(
      gl, GL_TEXTURE_2D, texture_id, true, false, IntPoint(0, 0),
      IntRect(IntPoint(0, 0), drawing_buffer->Size()), source_buffer);
}

void ImageBuffer::Draw(GraphicsContext& context,
                       const FloatRect& dest_rect,
                       const FloatRect* src_ptr,
                       SkBlendMode op) {
  if (!IsSurfaceValid())
    return;

  FloatRect src_rect =
      src_ptr ? *src_ptr : FloatRect(FloatPoint(), FloatSize(size()));
  surface_->Draw(context, dest_rect, src_rect, op);
}

void ImageBuffer::Flush(FlushReason reason) {
  if (surface_->Canvas()) {
    surface_->Flush(reason);
  }
}

void ImageBuffer::FlushGpu(FlushReason reason) {
  if (surface_->Canvas()) {
    surface_->FlushGpu(reason);
  }
}

bool ImageBuffer::GetImageData(Multiply multiplied,
                               const IntRect& rect,
                               WTF::ArrayBufferContents& contents) const {
  uint8_t bytes_per_pixel = 4;
  if (surface_->ColorSpace())
    bytes_per_pixel = SkColorTypeBytesPerPixel(surface_->ColorType());
  CheckedNumeric<int> data_size = bytes_per_pixel;
  data_size *= rect.Width();
  data_size *= rect.Height();
  if (!data_size.IsValid())
    return false;

  if (!IsSurfaceValid()) {
    size_t alloc_size_in_bytes = rect.Width() * rect.Height() * bytes_per_pixel;
    auto data = WTF::ArrayBufferContents::CreateDataHandle(
        alloc_size_in_bytes, WTF::ArrayBufferContents::kZeroInitialize);
    if (!data)
      return false;
    WTF::ArrayBufferContents result(std::move(data), alloc_size_in_bytes,
                                    WTF::ArrayBufferContents::kNotShared);
    result.Transfer(contents);
    return true;
  }

  DCHECK(Canvas());

  sk_sp<SkImage> snapshot = surface_->NewImageSnapshot(
      kPreferNoAcceleration, kSnapshotReasonGetImageData);
  if (!snapshot)
    return false;

  const bool may_have_stray_area =
      surface_->IsAccelerated()  // GPU readback may fail silently
      || rect.X() < 0 || rect.Y() < 0 ||
      rect.MaxX() > surface_->size().Width() ||
      rect.MaxY() > surface_->size().Height();
  size_t alloc_size_in_bytes = rect.Width() * rect.Height() * bytes_per_pixel;
  WTF::ArrayBufferContents::InitializationPolicy initialization_policy =
      may_have_stray_area ? WTF::ArrayBufferContents::kZeroInitialize
                          : WTF::ArrayBufferContents::kDontInitialize;
  auto data = WTF::ArrayBufferContents::CreateDataHandle(alloc_size_in_bytes,
                                                         initialization_policy);
  if (!data)
    return false;
  WTF::ArrayBufferContents result(std::move(data), alloc_size_in_bytes,
                                  WTF::ArrayBufferContents::kNotShared);

  // Skia does not support unpremultiplied read with an F16 to 8888 conversion
  bool use_f16_workaround = surface_->ColorType() == kRGBA_F16_SkColorType;

  SkAlphaType alpha_type = (multiplied == kPremultiplied || use_f16_workaround)
                               ? kPremul_SkAlphaType
                               : kUnpremul_SkAlphaType;
  // The workaround path use a canvas draw under the hood, which can only
  // use N32 at this time.
  SkColorType color_type =
      use_f16_workaround ? kN32_SkColorType : kRGBA_8888_SkColorType;

  // Only use sRGB when the surface has a color space.  Converting untagged
  // pixels to a particular color space is not well-defined in Skia.
  sk_sp<SkColorSpace> color_space = nullptr;
  if (surface_->ColorSpace()) {
    color_space = SkColorSpace::MakeSRGB();
  }

  SkImageInfo info = SkImageInfo::Make(rect.Width(), rect.Height(), color_type,
                                       alpha_type, std::move(color_space));

  snapshot->readPixels(info, result.Data(), bytes_per_pixel * rect.Width(),
                       rect.X(), rect.Y());
  gpu_readback_invoked_in_current_frame_ = true;

  if (use_f16_workaround) {
    uint32_t* pixel = (uint32_t*)result.Data();
    size_t pixel_count = alloc_size_in_bytes / sizeof(uint32_t);
    // TODO(skbug.com/5853): make readPixels support RGBA output so that we no
    // longer
    // have to do this.
    if (kN32_SkColorType == kBGRA_8888_SkColorType) {
      // Convert BGRA to RGBA if necessary on this platform.
      SkSwapRB(pixel, pixel, pixel_count);
    }
    // TODO(skbug.com/5853): We should really be doing the unpremultiply in
    // linear space
    // and skia should provide that service.
    if (multiplied == kUnmultiplied) {
      for (; pixel_count; --pixel_count) {
        *pixel = SkUnPreMultiply::UnPreMultiplyPreservingByteOrder(*pixel);
        ++pixel;
      }
    }
  }

  result.Transfer(contents);
  return true;
}

void ImageBuffer::PutByteArray(Multiply multiplied,
                               const unsigned char* source,
                               const IntSize& source_size,
                               const IntRect& source_rect,
                               const IntPoint& dest_point) {
  if (!IsSurfaceValid())
    return;
  uint8_t bytes_per_pixel = 4;
  if (surface_->ColorSpace())
    bytes_per_pixel = SkColorTypeBytesPerPixel(surface_->ColorType());

  DCHECK_GT(source_rect.Width(), 0);
  DCHECK_GT(source_rect.Height(), 0);

  int origin_x = source_rect.X();
  int dest_x = dest_point.X() + source_rect.X();
  DCHECK_GE(dest_x, 0);
  DCHECK_LT(dest_x, surface_->size().Width());
  DCHECK_GE(origin_x, 0);
  DCHECK_LT(origin_x, source_rect.MaxX());

  int origin_y = source_rect.Y();
  int dest_y = dest_point.Y() + source_rect.Y();
  DCHECK_GE(dest_y, 0);
  DCHECK_LT(dest_y, surface_->size().Height());
  DCHECK_GE(origin_y, 0);
  DCHECK_LT(origin_y, source_rect.MaxY());

  const size_t src_bytes_per_row = bytes_per_pixel * source_size.Width();
  const void* src_addr =
      source + origin_y * src_bytes_per_row + origin_x * bytes_per_pixel;

  SkAlphaType alpha_type;
  if (kOpaque == surface_->GetOpacityMode()) {
    // If the surface is opaque, tell it that we are writing opaque
    // pixels.  Writing non-opaque pixels to opaque is undefined in
    // Skia.  There is some discussion about whether it should be
    // defined in skbug.com/6157.  For now, we can get the desired
    // behavior (memcpy) by pretending the write is opaque.
    alpha_type = kOpaque_SkAlphaType;
  } else {
    alpha_type = (multiplied == kPremultiplied) ? kPremul_SkAlphaType
                                                : kUnpremul_SkAlphaType;
  }

  SkImageInfo info;
  if (surface_->ColorSpace()) {
    info = SkImageInfo::Make(source_rect.Width(), source_rect.Height(),
                             surface_->ColorType(), alpha_type,
                             surface_->ColorSpace());
  } else {
    info = SkImageInfo::Make(source_rect.Width(), source_rect.Height(),
                             kRGBA_8888_SkColorType, alpha_type,
                             SkColorSpace::MakeSRGB());
  }
  surface_->WritePixels(info, src_addr, src_bytes_per_row, dest_x, dest_y);
}

void ImageBuffer::UpdateGPUMemoryUsage() const {
  if (this->IsAccelerated()) {
    // If image buffer is accelerated, we should keep track of GPU memory usage.
    int gpu_buffer_count = 2;
    CheckedNumeric<intptr_t> checked_gpu_usage =
        SkColorTypeBytesPerPixel(surface_->ColorType()) * gpu_buffer_count;
    checked_gpu_usage *= this->size().Width();
    checked_gpu_usage *= this->size().Height();
    intptr_t gpu_memory_usage =
        checked_gpu_usage.ValueOrDefault(std::numeric_limits<intptr_t>::max());

    if (!gpu_memory_usage_)  // was not accelerated before
      global_accelerated_image_buffer_count_++;

    global_gpu_memory_usage_ += (gpu_memory_usage - gpu_memory_usage_);
    gpu_memory_usage_ = gpu_memory_usage;
  } else if (gpu_memory_usage_) {
    // In case of switching from accelerated to non-accelerated mode,
    // the GPU memory usage needs to be updated too.
    DCHECK_GT(global_accelerated_image_buffer_count_, 0u);
    global_accelerated_image_buffer_count_--;
    global_gpu_memory_usage_ -= gpu_memory_usage_;
    gpu_memory_usage_ = 0;

    if (client_)
      client_->DidDisableAcceleration();
  }
}

namespace {

class UnacceleratedSurfaceFactory
    : public RecordingImageBufferFallbackSurfaceFactory {
 public:
  virtual std::unique_ptr<ImageBufferSurface> CreateSurface(
      const IntSize& size,
      OpacityMode opacity_mode,
      sk_sp<SkColorSpace> color_space,
      SkColorType color_type) {
    return WTF::WrapUnique(new UnacceleratedImageBufferSurface(
        size, opacity_mode, kInitializeImagePixels, std::move(color_space),
        color_type));
  }

  virtual ~UnacceleratedSurfaceFactory() {}
};

}  // namespace

void ImageBuffer::DisableAcceleration() {
  if (!IsAccelerated())
    return;

  // Create and configure a recording (unaccelerated) surface.
  std::unique_ptr<RecordingImageBufferFallbackSurfaceFactory> surface_factory =
      WTF::MakeUnique<UnacceleratedSurfaceFactory>();
  std::unique_ptr<ImageBufferSurface> surface =
      WTF::WrapUnique(new RecordingImageBufferSurface(
          surface_->size(), std::move(surface_factory),
          surface_->GetOpacityMode(), surface_->ColorSpace(),
          surface_->ColorType()));
  SetSurface(std::move(surface));
}

void ImageBuffer::SetSurface(std::unique_ptr<ImageBufferSurface> surface) {
  sk_sp<SkImage> image =
      surface_->NewImageSnapshot(kPreferNoAcceleration, kSnapshotReasonPaint);

  // image can be null if alloaction failed in which case we should just
  // abort the surface switch to reatain the old surface which is still
  // functional.
  if (!image)
    return;

  if (surface->IsRecording()) {
    // Using a GPU-backed image with RecordingImageBufferSurface
    // will fail at playback time.
    image = image->makeNonTextureImage();
  }
  surface->Canvas()->drawImage(std::move(image), 0, 0);

  surface->SetImageBuffer(this);
  if (client_)
    client_->RestoreCanvasMatrixClipStack(surface->Canvas());
  surface_ = std::move(surface);

  UpdateGPUMemoryUsage();
}

bool ImageDataBuffer::EncodeImage(const String& mime_type,
                                  const double& quality,
                                  Vector<unsigned char>* encoded_image) const {
  if (mime_type == "image/jpeg") {
    if (!JPEGImageEncoder::Encode(*this, quality, encoded_image))
      return false;
  } else if (mime_type == "image/webp") {
    int compression_quality = WEBPImageEncoder::kDefaultCompressionQuality;
    if (quality >= 0.0 && quality <= 1.0)
      compression_quality = static_cast<int>(quality * 100 + 0.5);
    if (!WEBPImageEncoder::Encode(*this, compression_quality, encoded_image))
      return false;
  } else {
    if (!PNGImageEncoder::Encode(*this, encoded_image))
      return false;
    DCHECK_EQ(mime_type, "image/png");
  }

  return true;
}

String ImageDataBuffer::ToDataURL(const String& mime_type,
                                  const double& quality) const {
  DCHECK(MIMETypeRegistry::IsSupportedImageMIMETypeForEncoding(mime_type));

  Vector<unsigned char> result;
  if (!EncodeImage(mime_type, quality, &result))
    return "data:,";

  return "data:" + mime_type + ";base64," + Base64Encode(result);
}

}  // namespace blink
