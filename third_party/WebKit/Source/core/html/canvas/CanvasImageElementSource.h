// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CanvasImageElementSource_h
#define CanvasImageElementSource_h

#include "core/CoreExport.h"
#include "core/html/canvas/CanvasImageSource.h"

namespace blink {

class Element;
class ImageLoader;
class ImageResourceContent;

class CORE_EXPORT CanvasImageElementSource : public CanvasImageSource {
 public:
  virtual ImageLoader& GetImageLoader() const = 0;
  virtual FloatSize SourceDefaultObjectSize() = 0;

  PassRefPtr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const override;

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

 private:
  ImageResourceContent* CachedImage() const;
  const Element& GetElement() const;
};

}  // namespace blink

#endif  // CanvasImageElementSource_h
