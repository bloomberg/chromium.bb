/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StyleFetchedImageSet_h
#define StyleFetchedImageSet_h

#include "core/loader/resource/ImageResourceObserver.h"
#include "core/style/StyleImage.h"
#include "platform/geometry/LayoutSize.h"
#include "platform/weborigin/KURL.h"

namespace blink {

class CSSImageSetValue;
class ImageResourceObserver;

// This class keeps one cached image and has access to a set of alternatives.

class StyleFetchedImageSet final : public StyleImage,
                                   public ImageResourceObserver {
  USING_PRE_FINALIZER(StyleFetchedImageSet, Dispose);

 public:
  static StyleFetchedImageSet* Create(ImageResourceContent* image,
                                      float image_scale_factor,
                                      CSSImageSetValue* value,
                                      const KURL& url) {
    return new StyleFetchedImageSet(image, image_scale_factor, value, url);
  }
  ~StyleFetchedImageSet() override;

  CSSValue* CssValue() const override;
  CSSValue* ComputedCSSValue() const override;

  // FIXME: This is used by StyleImage for equals comparison, but this
  // implementation only looks at the image from the set that we have loaded.
  // I'm not sure if that is meaningful enough or not.
  WrappedImagePtr Data() const override;

  bool CanRender() const override;
  bool IsLoaded() const override;
  bool ErrorOccurred() const override;
  LayoutSize ImageSize(const Document&,
                       float multiplier,
                       const LayoutSize& default_object_size) const override;
  bool ImageHasRelativeSize() const override;
  bool UsesImageContainerSize() const override;
  void AddClient(ImageResourceObserver*) override;
  void RemoveClient(ImageResourceObserver*) override;
  RefPtr<Image> GetImage(const ImageResourceObserver&,
                         const Document&,
                         const ComputedStyle&,
                         const IntSize& container_size,
                         const LayoutSize* logical_size) const override;
  float ImageScaleFactor() const override { return image_scale_factor_; }
  bool KnownToBeOpaque(const Document&, const ComputedStyle&) const override;
  ImageResourceContent* CachedImage() const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  StyleFetchedImageSet(ImageResourceContent*,
                       float image_scale_factor,
                       CSSImageSetValue*,
                       const KURL&);

  void Dispose();

  String DebugName() const override { return "StyleFetchedImageSet"; }

  Member<ImageResourceContent> best_fit_image_;
  float image_scale_factor_;

  Member<CSSImageSetValue> image_set_value_;  // Not retained; it owns us.
  const KURL url_;
};

DEFINE_STYLE_IMAGE_TYPE_CASTS(StyleFetchedImageSet, IsImageResourceSet());

}  // namespace blink

#endif  // StyleFetchedImageSet_h
