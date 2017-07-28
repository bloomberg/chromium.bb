// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageBitmap_h
#define ImageBitmap_h

#include <memory>
#include "core/CoreExport.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/html/canvas/ImageElementBase.h"
#include "core/imagebitmap/ImageBitmapOptions.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/Image.h"
#include "platform/graphics/ImageBuffer.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/RefPtr.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {
class Document;
class HTMLCanvasElement;
class HTMLVideoElement;
class ImageData;
class ImageDecoder;
class OffscreenCanvas;

enum AlphaDisposition {
  kPremultiplyAlpha,
  kDontPremultiplyAlpha,
};
enum DataColorFormat {
  kRGBAColorType,
  kN32ColorType,
};
enum ColorSpaceInfoUpdate {
  kUpdateColorSpaceInformation,
  kDontUpdateColorSpaceInformation,
};

class CORE_EXPORT ImageBitmap final
    : public GarbageCollectedFinalized<ImageBitmap>,
      public ScriptWrappable,
      public CanvasImageSource,
      public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ImageBitmap* Create(ImageElementBase*,
                             Optional<IntRect>,
                             Document*,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* Create(HTMLVideoElement*,
                             Optional<IntRect>,
                             Document*,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* Create(HTMLCanvasElement*,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* Create(OffscreenCanvas*,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* Create(ImageData*,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* Create(ImageBitmap*,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  static ImageBitmap* Create(RefPtr<StaticBitmapImage>);
  static ImageBitmap* Create(RefPtr<StaticBitmapImage>,
                             Optional<IntRect>,
                             const ImageBitmapOptions& = ImageBitmapOptions());
  // This function is called by structured-cloning an ImageBitmap.
  // isImageBitmapPremultiplied indicates whether the original ImageBitmap is
  // premultiplied or not.
  // isImageBitmapOriginClean indicates whether the original ImageBitmap is
  // origin clean or not.
  static ImageBitmap* Create(const void* pixel_data,
                             uint32_t width,
                             uint32_t height,
                             bool is_image_bitmap_premultiplied,
                             bool is_image_bitmap_origin_clean,
                             const CanvasColorParams&);
  static ScriptPromise CreateAsync(
      ImageElementBase*,
      Optional<IntRect>,
      Document*,
      ScriptState*,
      const ImageBitmapOptions& = ImageBitmapOptions());
  static sk_sp<SkImage> GetSkImageFromDecoder(std::unique_ptr<ImageDecoder>);
  static bool IsResizeOptionValid(const ImageBitmapOptions&, ExceptionState&);
  static bool IsSourceSizeValid(int source_width,
                                int source_height,
                                ExceptionState&);

  // Type and helper function required by CallbackPromiseAdapter:
  using WebType = sk_sp<SkImage>;
  static ImageBitmap* Take(ScriptPromiseResolver*, sk_sp<SkImage>);

  RefPtr<StaticBitmapImage> BitmapImage() const { return image_; }
  RefPtr<Uint8Array> CopyBitmapData();
  RefPtr<Uint8Array> CopyBitmapData(AlphaDisposition,
                                    DataColorFormat = kRGBAColorType);
  unsigned long width() const;
  unsigned long height() const;
  IntSize Size() const;

  bool IsNeutered() const { return is_neutered_; }
  bool OriginClean() const { return image_->OriginClean(); }
  bool IsPremultiplied() const { return image_->IsPremultiplied(); }
  RefPtr<StaticBitmapImage> Transfer();
  void close();

  ~ImageBitmap() override;

  CanvasColorParams GetCanvasColorParams();

  // CanvasImageSource implementation
  RefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                        AccelerationHint,
                                        SnapshotReason,
                                        const FloatSize&) override;
  bool WouldTaintOrigin(SecurityOrigin*) const override {
    return !image_->OriginClean();
  }
  void AdjustDrawRects(FloatRect* src_rect, FloatRect* dst_rect) const override;
  FloatSize ElementSize(const FloatSize&) const override;
  bool IsImageBitmap() const override { return true; }
  int SourceWidth() override { return image_ ? image_->width() : 0; }
  int SourceHeight() override { return image_ ? image_->height() : 0; }
  bool IsAccelerated() const override;

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override { return Size(); }
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect>,
                                  const ImageBitmapOptions&,
                                  ExceptionState&) override;

  struct ParsedOptions {
    bool flip_y = false;
    bool premultiply_alpha = true;
    bool should_scale_input = false;
    unsigned resize_width = 0;
    unsigned resize_height = 0;
    IntRect crop_rect;
    SkFilterQuality resize_quality = kLow_SkFilterQuality;
    CanvasColorParams color_params;
    bool color_canvas_extensions_enabled = false;
  };

  DECLARE_VIRTUAL_TRACE();

 private:
  ImageBitmap(ImageElementBase*,
              Optional<IntRect>,
              Document*,
              const ImageBitmapOptions&);
  ImageBitmap(HTMLVideoElement*,
              Optional<IntRect>,
              Document*,
              const ImageBitmapOptions&);
  ImageBitmap(HTMLCanvasElement*, Optional<IntRect>, const ImageBitmapOptions&);
  ImageBitmap(OffscreenCanvas*, Optional<IntRect>, const ImageBitmapOptions&);
  ImageBitmap(ImageData*, Optional<IntRect>, const ImageBitmapOptions&);
  ImageBitmap(ImageBitmap*, Optional<IntRect>, const ImageBitmapOptions&);
  ImageBitmap(RefPtr<StaticBitmapImage>);
  ImageBitmap(RefPtr<StaticBitmapImage>,
              Optional<IntRect>,
              const ImageBitmapOptions&);
  ImageBitmap(const void* pixel_data,
              uint32_t width,
              uint32_t height,
              bool is_image_bitmap_premultiplied,
              bool is_image_bitmap_origin_clean,
              const CanvasColorParams&);
  static void ResolvePromiseOnOriginalThread(ScriptPromiseResolver*,
                                             sk_sp<SkImage>,
                                             bool origin_clean,
                                             std::unique_ptr<ParsedOptions>);
  static void RasterizeImageOnBackgroundThread(ScriptPromiseResolver*,
                                               sk_sp<PaintRecord>,
                                               const IntRect&,
                                               bool origin_clean,
                                               std::unique_ptr<ParsedOptions>);
  RefPtr<StaticBitmapImage> image_;
  bool is_neutered_ = false;
};

}  // namespace blink

#endif  // ImageBitmap_h
