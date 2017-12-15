// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/StaticBitmapImage.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "platform/graphics/AcceleratedStaticBitmapImage.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/ImageObserver.h"
#include "platform/graphics/UnacceleratedStaticBitmapImage.h"
#include "platform/graphics/paint/PaintImage.h"
#include "platform/wtf/CheckedNumeric.h"
#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  if (image->isTextureBacked()) {
    CHECK(context_provider_wrapper);
    return AcceleratedStaticBitmapImage::CreateFromSkImage(
        image, std::move(context_provider_wrapper));
  }
  return UnacceleratedStaticBitmapImage::Create(image);
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(PaintImage image) {
  DCHECK(!image.GetSkImage()->isTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(image));
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    scoped_refptr<Uint8Array>&& image_pixels,
    const SkImageInfo& info) {
  SkPixmap pixmap(info, image_pixels->Data(), info.minRowBytes());

  Uint8Array* pixels = image_pixels.get();
  if (pixels) {
    pixels->AddRef();
    image_pixels = nullptr;
  }

  return Create(SkImage::MakeFromRaster(
      pixmap,
      [](const void*, void* p) { static_cast<Uint8Array*>(p)->Release(); },
      pixels));
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    WTF::ArrayBufferContents& contents,
    const SkImageInfo& info) {
  SkPixmap pixmap(info, contents.Data(), info.minRowBytes());
  return Create(SkImage::MakeFromRaster(pixmap, nullptr, nullptr));
}

void StaticBitmapImage::DrawHelper(PaintCanvas* canvas,
                                   const PaintFlags& flags,
                                   const FloatRect& dst_rect,
                                   const FloatRect& src_rect,
                                   ImageClampingMode clamp_mode,
                                   const PaintImage& image) {
  FloatRect adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(SkRect::MakeWH(image.width(), image.height()));

  if (dst_rect.IsEmpty() || adjusted_src_rect.IsEmpty())
    return;  // Nothing to draw.

  canvas->drawImageRect(image, adjusted_src_rect, dst_rect, &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::ConvertToColorSpace(
    sk_sp<SkColorSpace> target,
    SkTransferFunctionBehavior transfer_function_behavior) {
  sk_sp<SkImage> skia_image = PaintImageForCurrentFrame().GetSkImage();
  sk_sp<SkColorSpace> src_color_space = skia_image->refColorSpace();
  if (!src_color_space.get())
    src_color_space = SkColorSpace::MakeSRGB();
  sk_sp<SkColorSpace> dst_color_space = target;
  if (!dst_color_space.get())
    dst_color_space = SkColorSpace::MakeSRGB();
  if (SkColorSpace::Equals(src_color_space.get(), dst_color_space.get()))
    return this;

  sk_sp<SkImage> converted_skia_image =
      skia_image->makeColorSpace(dst_color_space, transfer_function_behavior);
  DCHECK(converted_skia_image.get());
  DCHECK(skia_image.get() != converted_skia_image.get());

  return StaticBitmapImage::Create(converted_skia_image,
                                   ContextProviderWrapper());
}

bool StaticBitmapImage::ConvertToArrayBufferContents(
    scoped_refptr<StaticBitmapImage> src_image,
    WTF::ArrayBufferContents& dest_contents,
    const IntRect& rect,
    const CanvasColorParams& color_params,
    bool is_accelerated) {
  uint8_t bytes_per_pixel = color_params.BytesPerPixel();
  CheckedNumeric<int> data_size = bytes_per_pixel;
  data_size *= rect.Size().Area();
  if (!data_size.IsValid() ||
      data_size.ValueOrDie() > v8::TypedArray::kMaxLength)
    return false;

  size_t alloc_size_in_bytes = rect.Size().Area() * bytes_per_pixel;
  if (!src_image) {
    auto data = WTF::ArrayBufferContents::CreateDataHandle(
        alloc_size_in_bytes, WTF::ArrayBufferContents::kZeroInitialize);
    if (!data)
      return false;
    WTF::ArrayBufferContents result(std::move(data), alloc_size_in_bytes,
                                    WTF::ArrayBufferContents::kNotShared);
    result.Transfer(dest_contents);
    return true;
  }

  const bool may_have_stray_area =
      is_accelerated  // GPU readback may fail silently
      || rect.X() < 0 || rect.Y() < 0 ||
      rect.MaxX() > src_image->Size().Width() ||
      rect.MaxY() > src_image->Size().Height();
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
      (color_params.GetSkColorType() == kRGBA_F16_SkColorType)
          ? kRGBA_F16_SkColorType
          : kRGBA_8888_SkColorType;
  SkImageInfo info = SkImageInfo::Make(rect.Width(), rect.Height(), color_type,
                                       kUnpremul_SkAlphaType);
  sk_sp<SkImage> sk_image = src_image->PaintImageForCurrentFrame().GetSkImage();
  bool read_pixels_successful = sk_image->readPixels(
      info, result.Data(), info.minRowBytes(), rect.X(), rect.Y());
  DCHECK(read_pixels_successful ||
         !sk_image->bounds().intersect(SkIRect::MakeXYWH(
             rect.X(), rect.Y(), info.width(), info.height())));
  result.Transfer(dest_contents);
  return true;
}

const gpu::SyncToken& StaticBitmapImage::GetSyncToken() const {
  static const gpu::SyncToken sync_token;
  return sync_token;
}

bool StaticBitmapImage::CopyImageToPlatformTexture(
    gpu::gles2::GLES2Interface* gl,
    GLenum target,
    GLuint texture,
    bool premultiply_alpha,
    bool flip_y,
    const IntPoint& dest_point,
    const IntRect& source_sub_rectangle) {
  if (!IsTextureBacked() || !IsValid())
    return false;

  // Get the texture ID, flushing pending operations if needed.
  const GrGLTextureInfo* texture_info = skia::GrBackendObjectToGrGLTextureInfo(
      PaintImageForCurrentFrame().GetSkImage()->getTextureHandle(true));
  if (!texture_info || !texture_info->fID)
    return false;

  WebGraphicsContext3DProvider* provider = ContextProvider();
  if (!provider || !provider->GetGrContext())
    return false;
  gpu::gles2::GLES2Interface* source_gl = provider->ContextGL();

  gpu::Mailbox mailbox;

  // Contexts may be in a different share group. We must transfer the texture
  // through a mailbox first.
  source_gl->GenMailboxCHROMIUM(mailbox.name);
  source_gl->ProduceTextureDirectCHROMIUM(texture_info->fID, mailbox.name);
  const GLuint64 shared_fence_sync = source_gl->InsertFenceSyncCHROMIUM();
  // TODO(xlai): This one might need to be replaced with ShallowFlushCHROMIUM().
  // See crbug.com/794706.
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

}  // namespace blink
