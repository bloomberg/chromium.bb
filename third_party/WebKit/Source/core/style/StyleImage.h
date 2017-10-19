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
class IntSize;
class LayoutSize;
class SVGImage;
class Document;
class ComputedStyle;
class ImageResourceObserver;

typedef void* WrappedImagePtr;

class CORE_EXPORT StyleImage : public GarbageCollectedFinalized<StyleImage> {
 public:
  virtual ~StyleImage();

  bool operator==(const StyleImage& other) const {
    return Data() == other.Data();
  }

  virtual CSSValue* CssValue() const = 0;
  virtual CSSValue* ComputedCSSValue() const = 0;

  virtual bool CanRender() const { return true; }
  virtual bool IsLoaded() const { return true; }
  virtual bool ErrorOccurred() const { return false; }
  // Note that the defaultObjectSize is assumed to be in the
  // effective zoom level given by multiplier, i.e. if multiplier is
  // the constant 1 the defaultObjectSize should be unzoomed.
  virtual LayoutSize ImageSize(const Document&,
                               float multiplier,
                               const LayoutSize& default_object_size) const = 0;
  virtual bool ImageHasRelativeSize() const = 0;
  virtual bool UsesImageContainerSize() const = 0;
  virtual void AddClient(ImageResourceObserver*) = 0;
  virtual void RemoveClient(ImageResourceObserver*) = 0;
  // Note that the container_size is in the effective zoom level of
  // the style that applies to the given ImageResourceObserver, i.e if the zoom
  // level is 1.0 the container_size should be unzoomed.
  // The |logical_size| is the |container_size| without applying subpixel
  // snapping. Both sizes include zoom. This size is only currently computed for
  // BoxPainterBase and NinePieceImagePainter as these are the only painters
  // which use custom paint. We pass a nullptr for other subclasses.
  // TODO(schenney): Pass the |container_size| unsnapped as a LayoutSize so that
  // we don't need to pass an additional parameter.
  virtual RefPtr<Image> GetImage(const ImageResourceObserver&,
                                 const Document&,
                                 const ComputedStyle&,
                                 const IntSize& container_size,
                                 const LayoutSize* logical_size) const = 0;
  virtual WrappedImagePtr Data() const = 0;
  virtual float ImageScaleFactor() const { return 1; }
  virtual bool KnownToBeOpaque(const Document&, const ComputedStyle&) const = 0;
  virtual ImageResourceContent* CachedImage() const { return 0; }

  ALWAYS_INLINE bool IsImageResource() const { return is_image_resource_; }
  ALWAYS_INLINE bool IsPendingImage() const { return is_pending_image_; }
  ALWAYS_INLINE bool IsGeneratedImage() const { return is_generated_image_; }
  ALWAYS_INLINE bool IsImageResourceSet() const {
    return is_image_resource_set_;
  }
  ALWAYS_INLINE bool IsInvalidImage() const { return is_invalid_image_; }
  ALWAYS_INLINE bool IsPaintImage() const { return is_paint_image_; }

  // If the StyleImage belongs to a User Agent stylesheet, it should be flagged
  // so that it can persist beyond a navigation.
  void FlagAsUserAgentResource();

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

  bool is_ua_css_resource_ = false;

  static LayoutSize ApplyZoom(const LayoutSize&, float multiplier);
  LayoutSize ImageSizeForSVGImage(SVGImage*,
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
