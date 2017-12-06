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
#include "platform/geometry/IntRect.h"
#include "platform/graphics/CanvasHeuristicParameters.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/UnacceleratedImageBufferSurface.h"
#include "platform/graphics/gpu/DrawingBuffer.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/graphics/paint/PaintRecord.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-encoders/ImageEncoder.h"
#include "platform/network/mime/MIMETypeRegistry.h"
#include "platform/runtime_enabled_features.h"
#include "platform/wtf/MathExtras.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/Base64.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSwizzle.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "v8/include/v8.h"

namespace blink {

std::unique_ptr<ImageBuffer> ImageBuffer::Create(
    std::unique_ptr<ImageBufferSurface> surface) {
  if (!surface->IsValid())
    return nullptr;
  return WTF::WrapUnique(new ImageBuffer(std::move(surface)));
}

std::unique_ptr<ImageBuffer> ImageBuffer::Create(
    const IntSize& size,
    ImageInitializationMode initialization_mode,
    const CanvasColorParams& color_params) {
  std::unique_ptr<ImageBufferSurface> surface(
      WTF::WrapUnique(new UnacceleratedImageBufferSurface(
          size, initialization_mode, color_params)));

  if (!surface->IsValid())
    return nullptr;
  return WTF::WrapUnique(new ImageBuffer(std::move(surface)));
}

ImageBuffer::ImageBuffer(std::unique_ptr<ImageBufferSurface> surface)
    : weak_ptr_factory_(this),
      snapshot_state_(kInitialSnapshotState),
      surface_(std::move(surface)) {
  surface_->SetImageBuffer(this);
}

ImageBuffer::~ImageBuffer() {
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

bool ImageBuffer::IsSurfaceValid() const {
  return surface_->IsValid();
}

void ImageBuffer::FinalizeFrame() {
  surface_->FinalizeFrame();
}

void ImageBuffer::DoPaintInvalidation(const FloatRect& dirty_rect) {
  surface_->DoPaintInvalidation(dirty_rect);
}

bool ImageBuffer::RestoreSurface() const {
  return surface_->IsValid() || surface_->Restore();
}

scoped_refptr<StaticBitmapImage> ImageBuffer::NewImageSnapshot(
    AccelerationHint hint,
    SnapshotReason reason) const {
  if (snapshot_state_ == kInitialSnapshotState)
    snapshot_state_ = kDidAcquireSnapshot;
  if (!IsSurfaceValid())
    return nullptr;
  return surface_->NewImageSnapshot(hint, reason);
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

  scoped_refptr<StaticBitmapImage> image =
      surface_->NewImageSnapshot(kPreferAcceleration, reason);
  if (!image || !image->IsTextureBacked() || !image->IsValid())
    return false;

  // Get the texture ID, flushing pending operations if needed.
  const GrGLTextureInfo* texture_info = skia::GrBackendObjectToGrGLTextureInfo(
      image->PaintImageForCurrentFrame().GetSkImage()->getTextureHandle(true));
  if (!texture_info || !texture_info->fID)
    return false;

  WebGraphicsContext3DProvider* provider = image->ContextProvider();
  if (!provider || !provider->GetGrContext())
    return false;
  gpu::gles2::GLES2Interface* source_gl = provider->ContextGL();

  gpu::Mailbox mailbox;

  // Contexts may be in a different share group. We must transfer the texture
  // through a mailbox first.
  source_gl->GenMailboxCHROMIUM(mailbox.name);
  source_gl->ProduceTextureDirectCHROMIUM(texture_info->fID, mailbox.name);
  const GLuint64 shared_fence_sync = source_gl->InsertFenceSyncCHROMIUM();
  source_gl->Flush();

  gpu::SyncToken produce_sync_token;
  source_gl->GenSyncTokenCHROMIUM(shared_fence_sync,
                                  produce_sync_token.GetData());
  gl->WaitSyncTokenCHROMIUM(produce_sync_token.GetConstData());

  GLuint source_texture = gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);

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
  source_gl->WaitSyncTokenCHROMIUM(copy_sync_token.GetConstData());
  // This disassociates the texture from the mailbox to avoid leaking the
  // mapping between the two in cases where the texture is recycled by skia.
  source_gl->ProduceTextureDirectCHROMIUM(0, mailbox.name);

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
  std::unique_ptr<WebGraphicsContext3DProvider> provider =
      Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider();
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
      src_ptr ? *src_ptr : FloatRect(FloatPoint(), FloatSize(Size()));
  surface_->Draw(context, dest_rect, src_rect, op);
}

bool ImageBuffer::GetImageData(const IntRect& rect,
                               WTF::ArrayBufferContents& contents,
                               bool* is_gpu_readback_invoked) const {
  uint8_t bytes_per_pixel = surface_->ColorParams().BytesPerPixel();
  CheckedNumeric<int> data_size = bytes_per_pixel;
  data_size *= rect.Size().Area();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
    return false;

  if (!IsSurfaceValid()) {
    size_t alloc_size_in_bytes = rect.Size().Area() * bytes_per_pixel;
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

  scoped_refptr<StaticBitmapImage> snapshot = surface_->NewImageSnapshot(
      kPreferNoAcceleration, kSnapshotReasonGetImageData);
  if (!snapshot)
    return false;

  const bool may_have_stray_area =
      surface_->IsAccelerated()  // GPU readback may fail silently
      || rect.X() < 0 || rect.Y() < 0 ||
      rect.MaxX() > surface_->Size().Width() ||
      rect.MaxY() > surface_->Size().Height();
  size_t alloc_size_in_bytes = rect.Size().Area() * bytes_per_pixel;
  WTF::ArrayBufferContents::InitializationPolicy initialization_policy =
      may_have_stray_area ? WTF::ArrayBufferContents::kZeroInitialize
                          : WTF::ArrayBufferContents::kDontInitialize;
  auto data = WTF::ArrayBufferContents::CreateDataHandle(alloc_size_in_bytes,
                                                         initialization_policy);
  if (!data)
    return false;
  WTF::ArrayBufferContents result(std::move(data), alloc_size_in_bytes,
                                  WTF::ArrayBufferContents::kNotShared);

  SkColorType color_type =
      (surface_->ColorParams().GetSkColorType() == kRGBA_F16_SkColorType)
          ? kRGBA_F16_SkColorType
          : kRGBA_8888_SkColorType;
  SkImageInfo info = SkImageInfo::Make(rect.Width(), rect.Height(), color_type,
                                       kUnpremul_SkAlphaType);
  sk_sp<SkImage> sk_image = snapshot->PaintImageForCurrentFrame().GetSkImage();
  bool read_pixels_successful = sk_image->readPixels(
      info, result.Data(), info.minRowBytes(), rect.X(), rect.Y());
  DCHECK(read_pixels_successful ||
         !sk_image->bounds().intersect(SkIRect::MakeXYWH(
             rect.X(), rect.Y(), info.width(), info.height())));
  if (is_gpu_readback_invoked) {
    *is_gpu_readback_invoked = true;
  }
  result.Transfer(contents);
  return true;
}

void ImageBuffer::PutByteArray(const unsigned char* source,
                               const IntSize& source_size,
                               const IntRect& source_rect,
                               const IntPoint& dest_point) {
  if (!IsSurfaceValid())
    return;
  uint8_t bytes_per_pixel = surface_->ColorParams().BytesPerPixel();

  DCHECK_GT(source_rect.Width(), 0);
  DCHECK_GT(source_rect.Height(), 0);

  int origin_x = source_rect.X();
  int dest_x = dest_point.X() + source_rect.X();
  DCHECK_GE(dest_x, 0);
  DCHECK_LT(dest_x, surface_->Size().Width());
  DCHECK_GE(origin_x, 0);
  DCHECK_LT(origin_x, source_rect.MaxX());

  int origin_y = source_rect.Y();
  int dest_y = dest_point.Y() + source_rect.Y();
  DCHECK_GE(dest_y, 0);
  DCHECK_LT(dest_y, surface_->Size().Height());
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
    alpha_type = kUnpremul_SkAlphaType;
  }

  SkImageInfo info;
  if (surface_->ColorParams().GetSkColorSpaceForSkSurfaces()) {
    info = SkImageInfo::Make(
        source_rect.Width(), source_rect.Height(),
        surface_->ColorParams().GetSkColorType(), alpha_type,
        surface_->ColorParams().GetSkColorSpaceForSkSurfaces());
    if (info.colorType() == kN32_SkColorType)
      info = info.makeColorType(kRGBA_8888_SkColorType);
  } else {
    info = SkImageInfo::Make(source_rect.Width(), source_rect.Height(),
                             kRGBA_8888_SkColorType, alpha_type);
  }
  surface_->WritePixels(info, src_addr, src_bytes_per_row, dest_x, dest_y);
}

void ImageBuffer::SetSurface(std::unique_ptr<ImageBufferSurface> surface) {
  scoped_refptr<StaticBitmapImage> image =
      surface_->NewImageSnapshot(kPreferNoAcceleration, kSnapshotReasonPaint);

  // image can be null if alloaction failed in which case we should just
  // abort the surface switch to reatain the old surface which is still
  // functional.
  if (!image)
    return;

  surface->Canvas()->drawImage(image->PaintImageForCurrentFrame(), 0, 0);
  surface->SetImageBuffer(this);
  surface_ = std::move(surface);
}

void ImageBuffer::OnCanvasDisposed() {
  if (surface_)
    surface_->SetCanvasResourceHost(nullptr);
}

const unsigned char* ImageDataBuffer::Pixels() const {
  if (uses_pixmap_)
    return static_cast<const unsigned char*>(pixmap_.addr());
  return data_;
}

ImageDataBuffer::ImageDataBuffer(scoped_refptr<StaticBitmapImage> image) {
  image_bitmap_ = image;
  sk_sp<SkImage> skia_image = image->PaintImageForCurrentFrame().GetSkImage();
  if (skia_image->isTextureBacked()) {
    skia_image = skia_image->makeNonTextureImage();
    image_bitmap_ = StaticBitmapImage::Create(skia_image);
  } else if (skia_image->isLazyGenerated()) {
    // Call readPixels() to trigger decoding.
    SkImageInfo info = SkImageInfo::MakeN32(1, 1, skia_image->alphaType());
    std::unique_ptr<uint8_t[]> pixel(new uint8_t[info.bytesPerPixel()]());
    skia_image->readPixels(info, pixel.get(), info.minRowBytes(), 1, 1);
  }
  bool peek_pixels = skia_image->peekPixels(&pixmap_);
  DCHECK(peek_pixels);
  uses_pixmap_ = true;
  size_ = IntSize(image->width(), image->height());
}

bool ImageDataBuffer::EncodeImage(const String& mime_type,
                                  const double& quality,
                                  Vector<unsigned char>* encoded_image) const {
  SkPixmap src;
  if (uses_pixmap_) {
    src = pixmap_;
  } else {
    SkImageInfo info =
        SkImageInfo::Make(Width(), Height(), kRGBA_8888_SkColorType,
                          kUnpremul_SkAlphaType, nullptr);
    const size_t rowBytes = info.minRowBytes();
    src.reset(info, Pixels(), rowBytes);
  }

  if (mime_type == "image/jpeg") {
    SkJpegEncoder::Options options;
    options.fQuality = ImageEncoder::ComputeJpegQuality(quality);
    options.fAlphaOption = SkJpegEncoder::AlphaOption::kBlendOnBlack;
    options.fBlendBehavior = SkTransferFunctionBehavior::kIgnore;
    if (options.fQuality == 100) {
      options.fDownsample = SkJpegEncoder::Downsample::k444;
    }
    return ImageEncoder::Encode(encoded_image, src, options);
  }

  if (mime_type == "image/webp") {
    SkWebpEncoder::Options options = ImageEncoder::ComputeWebpOptions(
        quality, SkTransferFunctionBehavior::kIgnore);
    return ImageEncoder::Encode(encoded_image, src, options);
  }

  DCHECK_EQ(mime_type, "image/png");
  SkPngEncoder::Options options;
  options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
  options.fZLibLevel = 3;
  options.fUnpremulBehavior = SkTransferFunctionBehavior::kIgnore;
  return ImageEncoder::Encode(encoded_image, src, options);
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
