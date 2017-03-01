// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleImageValue_h
#define CSSStyleImageValue_h

#include "core/CoreExport.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/cssom/CSSResourceValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/style/StyleImage.h"

namespace blink {

class CORE_EXPORT CSSStyleImageValue : public CSSResourceValue,
                                       public CanvasImageSource {
  WTF_MAKE_NONCOPYABLE(CSSStyleImageValue);
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~CSSStyleImageValue() {}

  double intrinsicWidth(bool& isNull) const;
  double intrinsicHeight(bool& isNull) const;
  double intrinsicRatio(bool& isNull);

  // CanvasImageSource
  bool isCSSImageValue() const final { return true; }
  int sourceWidth() final;
  int sourceHeight() final;
  bool wouldTaintOrigin(SecurityOrigin* destinationSecurityOrigin) const final {
    return true;
  }
  FloatSize elementSize(const FloatSize& defaultObjectSize) const final;
  PassRefPtr<Image> getSourceImageForCanvas(SourceImageStatus*,
                                            AccelerationHint,
                                            SnapshotReason,
                                            const FloatSize&) const final {
    return image();
  }
  bool isAccelerated() const override;

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->trace(m_imageValue);
    CSSResourceValue::trace(visitor);
  }

 protected:
  CSSStyleImageValue(const CSSImageValue* imageValue)
      : m_imageValue(imageValue) {}

  virtual LayoutSize imageLayoutSize() const {
    DCHECK(!isCachePending());
    ImageResourceContent* resourceContent =
        m_imageValue->cachedImage()->cachedImage();
    return resourceContent
               ? resourceContent->imageSize(DoNotRespectImageOrientation, 1,
                                            ImageResourceContent::IntrinsicSize)
               : LayoutSize(0, 0);
  }

  virtual bool isCachePending() const { return m_imageValue->isCachePending(); }

  ResourceStatus status() const override {
    if (isCachePending())
      return ResourceStatus::NotStarted;
    return m_imageValue->cachedImage()->cachedImage()->getStatus();
  }

  const CSSImageValue* cssImageValue() const { return m_imageValue.get(); };

 private:
  PassRefPtr<Image> image() const;

  Member<const CSSImageValue> m_imageValue;
};

}  // namespace blink

#endif  // CSSResourceValue_h
