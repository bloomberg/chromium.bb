// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleImageValue_h
#define CSSStyleImageValue_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/css/CSSImageValue.h"
#include "core/css/cssom/CSSResourceValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/html/canvas/CanvasImageSource.h"
#include "core/imagebitmap/ImageBitmapSource.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/style/StyleImage.h"

namespace blink {

// CSSStyleImageValue is the base class for Typed OM representations of images.
// The corresponding idl file is CSSImageValue.idl.
class CORE_EXPORT CSSStyleImageValue : public CSSResourceValue,
                                       public CanvasImageSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual ~CSSStyleImageValue() = default;

  double intrinsicWidth(bool& is_null) const;
  double intrinsicHeight(bool& is_null) const;
  double intrinsicRatio(bool& is_null);

  // CanvasImageSource
  bool IsCSSImageValue() const final { return true; }
  bool WouldTaintOrigin(
      SecurityOrigin* destination_security_origin) const final {
    return true;
  }
  FloatSize ElementSize(const FloatSize& default_object_size) const final;
  scoped_refptr<Image> GetSourceImageForCanvas(SourceImageStatus*,
                                               AccelerationHint,
                                               SnapshotReason,
                                               const FloatSize&) final {
    return GetImage();
  }
  bool IsAccelerated() const override;

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(image_value_);
    CSSResourceValue::Trace(visitor);
  }

 protected:
  CSSStyleImageValue(const CSSImageValue* image_value)
      : image_value_(image_value) {}

  virtual IntSize ImageSize() const {
    DCHECK(!IsCachePending());
    ImageResourceContent* resource_content =
        image_value_->CachedImage()->CachedImage();
    return resource_content
               ? resource_content->IntrinsicSize(kDoNotRespectImageOrientation)
               : IntSize(0, 0);
  }

  virtual bool IsCachePending() const { return image_value_->IsCachePending(); }

  ResourceStatus Status() const override {
    if (IsCachePending())
      return ResourceStatus::kNotStarted;
    return image_value_->CachedImage()->CachedImage()->GetContentStatus();
  }

  const CSSImageValue* CssImageValue() const { return image_value_.Get(); };

 private:
  scoped_refptr<Image> GetImage() const;

  Member<const CSSImageValue> image_value_;
  DISALLOW_COPY_AND_ASSIGN(CSSStyleImageValue);
};

}  // namespace blink

#endif  // CSSResourceValue_h
