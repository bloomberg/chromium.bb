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
  virtual ImageLoader& imageLoader() const = 0;
  virtual FloatSize sourceDefaultObjectSize() = 0;

  PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const override;

  bool wouldTaintOrigin(
      SecurityOrigin* destinationSecurityOrigin) const override;

  FloatSize elementSize(const FloatSize& defaultObjectSize) const override;
  FloatSize defaultDestinationSize(
      const FloatSize& defaultObjectSize) const override;

  bool isAccelerated() const override;

  int sourceWidth() override;
  int sourceHeight() override;

  bool isSVGSource() const override;

  bool isOpaque() const override;

  const KURL& sourceURL() const override;

 private:
  ImageResourceContent* cachedImage() const;
  const Element& element() const;
};

}  // namespace blink

#endif  // CanvasImageElementSource_h
