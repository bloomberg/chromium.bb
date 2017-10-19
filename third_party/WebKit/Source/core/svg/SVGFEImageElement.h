/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGFEImageElement_h
#define SVGFEImageElement_h

#include "core/loader/resource/ImageResourceObserver.h"
#include "core/svg/SVGAnimatedPreserveAspectRatio.h"
#include "core/svg/SVGFilterPrimitiveStandardAttributes.h"
#include "core/svg/SVGURIReference.h"
#include "platform/heap/Handle.h"

namespace blink {

class ImageResourceContent;

class SVGFEImageElement final : public SVGFilterPrimitiveStandardAttributes,
                                public SVGURIReference,
                                public ImageResourceObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGFEImageElement);

 public:
  DECLARE_NODE_FACTORY(SVGFEImageElement);

  bool CurrentFrameHasSingleSecurityOrigin() const;

  ~SVGFEImageElement() override;
  SVGAnimatedPreserveAspectRatio* preserveAspectRatio() {
    return preserve_aspect_ratio_.Get();
  }

  // Promptly remove as a ImageResource client.
  EAGERLY_FINALIZE();
  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGFEImageElement(Document&);

  void SvgAttributeChanged(const QualifiedName&) override;
  void ImageNotifyFinished(ImageResourceContent*) override;
  String DebugName() const override { return "SVGFEImageElement"; }

  FilterEffect* Build(SVGFilterBuilder*, Filter*) override;

  void ClearResourceReferences();
  void FetchImageResource();
  void ClearImageResource();

  void BuildPendingResource() override;
  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  Member<SVGAnimatedPreserveAspectRatio> preserve_aspect_ratio_;

  Member<ImageResourceContent> cached_image_;
  Member<IdTargetObserver> target_id_observer_;
};

}  // namespace blink

#endif  // SVGFEImageElement_h
