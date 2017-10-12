// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ImageElementBase_h
#define ImageElementBase_h

#include "core/CoreExport.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "platform/graphics/Image.h"

namespace blink {

class Element;
class ImageLoader;
class ImageResourceContent;

class CORE_EXPORT ImageElementBase : public CanvasImageSource,
                                     public ImageBitmapSource {
 public:
  virtual ImageLoader& GetImageLoader() const = 0;
  virtual FloatSize SourceDefaultObjectSize() = 0;

  // Parses the given async parameter value into an ImageDecodingMode. This is
  // used by SVGImageElement and HTMLImageElement since this class is a common
  // base for both elements.
  static Image::ImageDecodingMode ParseImageDecodingMode(const AtomicString&);

  IntSize BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  EventTarget&,
                                  Optional<IntRect>,
                                  const ImageBitmapOptions&) override;

  RefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                        AccelerationHint,
                                        SnapshotReason,
                                        const FloatSize&) override;

  bool WouldTaintOrigin(
      SecurityOrigin* destination_security_origin) const override;

  FloatSize ElementSize(const FloatSize& default_object_size) const override;
  FloatSize DefaultDestinationSize(
      const FloatSize& default_object_size) const override;

  bool IsAccelerated() const override;

  int SourceWidth() override;
  int SourceHeight() override;

  bool IsSVGSource() const override;

  bool IsOpaque() const override;

  const KURL& SourceURL() const override;

  ImageResourceContent* CachedImage() const;

 private:
  const Element& GetElement() const;
};

}  // namespace blink

#endif  // ImageElementBase_h
