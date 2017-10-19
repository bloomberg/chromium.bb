/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StylePendingImage_h
#define StylePendingImage_h

#include "core/css/CSSImageGeneratorValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSPaintValue.h"
#include "core/style/StyleImage.h"
#include "platform/graphics/Image.h"

namespace blink {

class ImageResourceObserver;

// StylePendingImage is a placeholder StyleImage that is entered into the
// ComputedStyle during style resolution, in order to avoid loading images that
// are not referenced by the final style.  They should never exist in a
// ComputedStyle after it has been returned from the style selector.

class StylePendingImage final : public StyleImage {
 public:
  static StylePendingImage* Create(const CSSValue& value) {
    return new StylePendingImage(value);
  }

  WrappedImagePtr Data() const override { return value_.Get(); }

  CSSValue* CssValue() const override { return value_; }

  CSSValue* ComputedCSSValue() const override {
    NOTREACHED();
    return nullptr;
  }

  CSSImageValue* CssImageValue() const {
    return value_->IsImageValue() ? ToCSSImageValue(value_.Get()) : 0;
  }
  CSSPaintValue* CssPaintValue() const {
    return value_->IsPaintValue() ? ToCSSPaintValue(value_.Get()) : 0;
  }
  CSSImageGeneratorValue* CssImageGeneratorValue() const {
    return value_->IsImageGeneratorValue()
               ? ToCSSImageGeneratorValue(value_.Get())
               : 0;
  }
  CSSImageSetValue* CssImageSetValue() const {
    return value_->IsImageSetValue() ? ToCSSImageSetValue(value_.Get()) : 0;
  }

  LayoutSize ImageSize(const Document&,
                       float /*multiplier*/,
                       const LayoutSize& /*defaultObjectSize*/) const override {
    return LayoutSize();
  }
  bool ImageHasRelativeSize() const override { return false; }
  bool UsesImageContainerSize() const override { return false; }
  void AddClient(ImageResourceObserver*) override {}
  void RemoveClient(ImageResourceObserver*) override {}
  RefPtr<Image> GetImage(const ImageResourceObserver&,
                         const Document&,
                         const ComputedStyle&,
                         const IntSize& container_size,
                         const LayoutSize* logical_size) const override {
    NOTREACHED();
    return nullptr;
  }
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const override {
    return false;
  }

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(value_);
    StyleImage::Trace(visitor);
  }

 private:
  explicit StylePendingImage(const CSSValue& value)
      : value_(const_cast<CSSValue*>(&value)) {
    is_pending_image_ = true;
  }

  // TODO(sashab): Replace this with <const CSSValue> once Member<>
  // supports const types.
  Member<CSSValue> value_;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StylePendingImage, IsPendingImage());

}  // namespace blink
#endif
