/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
 */

#ifndef SVGImageElement_h
#define SVGImageElement_h

#include "core/html/canvas/ImageElementBase.h"
#include "core/svg/SVGAnimatedLength.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGImageLoader.h"
#include "core/svg/SVGURIReference.h"
#include "platform/heap/Handle.h"

namespace blink {

class CORE_EXPORT SVGImageElement final : public SVGGraphicsElement,
                                          public ImageElementBase,
                                          public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGImageElement);

 public:
  DECLARE_NODE_FACTORY(SVGImageElement);
  DECLARE_VIRTUAL_TRACE();

  bool CurrentFrameHasSingleSecurityOrigin() const;

  SVGAnimatedLength* x() const { return x_.Get(); }
  SVGAnimatedLength* y() const { return y_.Get(); }
  SVGAnimatedLength* width() const { return width_.Get(); }
  SVGAnimatedLength* height() const { return height_.Get(); }
  SVGAnimatedPreserveAspectRatio* preserveAspectRatio() {
    return preserve_aspect_ratio_.Get();
  }

  // Exposed for testing.
  ImageResourceContent* CachedImage() const {
    return GetImageLoader().GetImage();
  }

 private:
  explicit SVGImageElement(Document&);

  bool IsStructurallyExternal() const override {
    return !HrefString().IsNull();
  }

  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  void SvgAttributeChanged(const QualifiedName&) override;

  void AttachLayoutTree(const AttachContext& = AttachContext()) override;
  InsertionNotificationRequest InsertedInto(ContainerNode*) override;

  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  const AtomicString ImageSourceURL() const override;

  bool HaveLoadedRequiredResources() override;

  bool SelfHasRelativeLengths() const override;
  void DidMoveToNewDocument(Document& old_document) override;
  SVGImageLoader& GetImageLoader() const override { return *image_loader_; }
  FloatSize SourceDefaultObjectSize() override;

  Member<SVGAnimatedLength> x_;
  Member<SVGAnimatedLength> y_;
  Member<SVGAnimatedLength> width_;
  Member<SVGAnimatedLength> height_;
  Member<SVGAnimatedPreserveAspectRatio> preserve_aspect_ratio_;

  Member<SVGImageLoader> image_loader_;
};

}  // namespace blink

#endif  // SVGImageElement_h
