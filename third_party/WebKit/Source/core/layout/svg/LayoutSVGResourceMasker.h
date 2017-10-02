/*
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#ifndef LayoutSVGResourceMasker_h
#define LayoutSVGResourceMasker_h

#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGMaskElement.h"
#include "core/svg/SVGUnitTypes.h"
#include "platform/geometry/FloatRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class AffineTransform;
class GraphicsContext;

class LayoutSVGResourceMasker final : public LayoutSVGResourceContainer {
 public:
  explicit LayoutSVGResourceMasker(SVGMaskElement*);
  ~LayoutSVGResourceMasker() override;

  const char* GetName() const override { return "LayoutSVGResourceMasker"; }

  void RemoveAllClientsFromCache(bool mark_for_invalidation = true) override;
  void RemoveClientFromCache(LayoutObject*,
                             bool mark_for_invalidation = true) override;

  FloatRect ResourceBoundingBox(const LayoutObject*);

  SVGUnitTypes::SVGUnitType MaskUnits() const {
    return ToSVGMaskElement(GetElement())
        ->maskUnits()
        ->CurrentValue()
        ->EnumValue();
  }
  SVGUnitTypes::SVGUnitType MaskContentUnits() const {
    return ToSVGMaskElement(GetElement())
        ->maskContentUnits()
        ->CurrentValue()
        ->EnumValue();
  }

  static const LayoutSVGResourceType kResourceType = kMaskerResourceType;
  LayoutSVGResourceType ResourceType() const override { return kResourceType; }

  sk_sp<const PaintRecord> CreatePaintRecord(AffineTransform&,
                                             const FloatRect&,
                                             GraphicsContext&);

 private:
  void CalculateMaskContentVisualRect();

  sk_sp<const PaintRecord> cached_paint_record_;
  FloatRect mask_content_boundaries_;
};

DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(LayoutSVGResourceMasker,
                                      kMaskerResourceType);

}  // namespace blink

#endif
