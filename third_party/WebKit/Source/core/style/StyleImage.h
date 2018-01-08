/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef StyleImage_h
#define StyleImage_h

#include "core/CoreExport.h"
#include "platform/graphics/Image.h"
#include "platform/wtf/Forward.h"

namespace blink {

class CSSValue;
class ImageResourceContent;
class LayoutSize;
class SVGImage;
class Document;
class ComputedStyle;
class ImageResourceObserver;

typedef void* WrappedImagePtr;

// This class represents a CSS <image> value in ComputedStyle. The underlying
// object can be an image, a gradient or anything else defined as an <image>
// value.
class CORE_EXPORT StyleImage : public GarbageCollectedFinalized<StyleImage> {
 public:
  virtual ~StyleImage() {}

  bool operator==(const StyleImage& other) const {
    return Data() == other.Data();
  }

  // Returns a CSSValue representing the origin <image> value. May not be the
  // actual CSSValue from which this StyleImage was originally created if the
  // CSSValue can be recreated easily (like for StyleFetchedImage and
  // StyleInvalidImage) and does not contain per-client state (like for
  // StyleGeneratedImage.)
  virtual CSSValue* CssValue() const = 0;

  // Returns a CSSValue suitable for using as part of a computed style
  // value. This often means that any URLs have been made absolute, and similar
  // actions described by a "Computed value" in the relevant specification.
  virtual CSSValue* ComputedCSSValue() const = 0;

  // An Image can be provided for rendering by GetImage.
  virtual bool CanRender() const { return true; }

  // All underlying resources this <image> references has finished loading.
  virtual bool IsLoaded() const { return true; }

  // Any underlying resources this <image> references failed to load.
  virtual bool ErrorOccurred() const { return false; }

  // Determine the concrete object size of this <image>, scaled by multiplier,
  // using the specified default object size. Return value as a FloatSize
  // because we want integer sizes to remain integers when zoomed and then
  // unzoomed. That is, (size * multiplier) / multiplier == size.
  //
  // The default object size is context dependent, see for instance the
  // "Examples of CSS Object Sizing" section of the CSS images specification.
  // https://drafts.csswg.org/css-images/#sizing
  //
  // The |default_object_size| is assumed to be in the effective zoom level
  // given by multiplier, i.e. if multiplier is 1 the |default_object_size| is
  // not zoomed.
  virtual FloatSize ImageSize(const Document&,
                              float multiplier,
                              const LayoutSize& default_object_size) const = 0;

  // The <image> does not have any intrinsic dimensions.
  virtual bool ImageHasRelativeSize() const = 0;

  // The <image> may depend on dimensions from the context the image is used in
  // (the "container".)
  virtual bool UsesImageContainerSize() const = 0;

  virtual void AddClient(ImageResourceObserver*) = 0;
  virtual void RemoveClient(ImageResourceObserver*) = 0;

  // Retrieve an Image representation for painting this <image>, using a
  // concrete object size (|container_size|.)
  //
  // Note that the |container_size| is in the effective zoom level of the
  // computed style, i.e if the style has an effective zoom level of 1.0 the
  // |container_size| is not zoomed.
  virtual scoped_refptr<Image> GetImage(
      const ImageResourceObserver&,
      const Document&,
      const ComputedStyle&,
      const LayoutSize& container_size) const = 0;

  // Opaque handle representing the underlying value of this <image>.
  virtual WrappedImagePtr Data() const = 0;

  // A scale factor indicating how many physical pixels in an image represent a
  // logical (CSS) pixel.
  virtual float ImageScaleFactor() const { return 1; }

  // Returns true if it can be determined that this <image> will always provide
  // an opaque Image.
  virtual bool KnownToBeOpaque(const Document&, const ComputedStyle&) const = 0;

  // If this <image> is intrinsically an image resource, this returns its
  // underlying ImageResourceContent, or otherwise nullptr.
  virtual ImageResourceContent* CachedImage() const { return nullptr; }

  ALWAYS_INLINE bool IsImageResource() const { return is_image_resource_; }
  ALWAYS_INLINE bool IsPendingImage() const { return is_pending_image_; }
  ALWAYS_INLINE bool IsGeneratedImage() const { return is_generated_image_; }
  ALWAYS_INLINE bool IsImageResourceSet() const {
    return is_image_resource_set_;
  }
  ALWAYS_INLINE bool IsInvalidImage() const { return is_invalid_image_; }
  ALWAYS_INLINE bool IsPaintImage() const { return is_paint_image_; }

  virtual void Trace(blink::Visitor* visitor) {}

 protected:
  StyleImage()
      : is_image_resource_(false),
        is_pending_image_(false),
        is_generated_image_(false),
        is_image_resource_set_(false),
        is_invalid_image_(false),
        is_paint_image_(false) {}
  bool is_image_resource_ : 1;
  bool is_pending_image_ : 1;
  bool is_generated_image_ : 1;
  bool is_image_resource_set_ : 1;
  bool is_invalid_image_ : 1;
  bool is_paint_image_ : 1;

  FloatSize ApplyZoom(const FloatSize&, float multiplier) const;
  FloatSize ImageSizeForSVGImage(SVGImage*,
                                 float multiplier,
                                 const LayoutSize& default_object_size) const;
};

#define DEFINE_STYLE_IMAGE_TYPE_CASTS(thisType, function)                   \
  DEFINE_TYPE_CASTS(thisType, StyleImage, styleImage, styleImage->function, \
                    styleImage.function);                                   \
  inline thisType* To##thisType(const Member<StyleImage>& styleImage) {     \
    return To##thisType(styleImage.Get());                                  \
  }                                                                         \
  typedef int NeedsSemiColonAfterDefineStyleImageTypeCasts

}  // namespace blink
#endif
