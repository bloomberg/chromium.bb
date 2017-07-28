// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleInvalidImage_h
#define StyleInvalidImage_h

#include "core/css/CSSImageValue.h"
#include "core/style/StyleImage.h"

namespace blink {

class ImageResourceObserver;

class StyleInvalidImage final : public StyleImage {
 public:
  static StyleInvalidImage* Create(const String& url) {
    return new StyleInvalidImage(url);
  }

  WrappedImagePtr Data() const override { return url_.Impl(); }

  CSSValue* CssValue() const override {
    return CSSImageValue::Create(AtomicString(url_));
  }

  CSSValue* ComputedCSSValue() const override { return CssValue(); }

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
                         const IntSize&) const override {
    return nullptr;
  }
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const override {
    return false;
  }

  DEFINE_INLINE_VIRTUAL_TRACE() { StyleImage::Trace(visitor); }

 private:
  explicit StyleInvalidImage(const String& url) : url_(url) {
    is_invalid_image_ = true;
  }

  String url_;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StyleInvalidImage, IsInvalidImage());

}  // namespace blink
#endif
