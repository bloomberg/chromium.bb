// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/CSSMaskPainter.h"

#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"

namespace blink {

Optional<IntRect> CSSMaskPainter::MaskBoundingBox(
    const LayoutObject& object,
    const LayoutPoint& paint_offset) {
  if (!object.IsBoxModelObject() && !object.IsSVGChild())
    return WTF::nullopt;

  if (object.IsSVG()) {
    SVGResources* resources =
        SVGResourcesCache::CachedResourcesForLayoutObject(object);
    LayoutSVGResourceMasker* masker = resources ? resources->Masker() : nullptr;
    if (masker)
      return EnclosingIntRect(masker->ResourceBoundingBox(&object));
  }

  if (object.IsSVGChild() && !object.IsSVGForeignObject())
    return WTF::nullopt;

  const ComputedStyle& style = object.StyleRef();
  if (!style.HasMask())
    return WTF::nullopt;

  LayoutRect maximum_mask_region;
  // For HTML/CSS objects, the extent of the mask is known as "mask
  // painting area", which is determined by CSS mask-clip property.
  // We don't implement mask-clip:margin-box or no-clip currently,
  // so the maximum we can get is border-box.
  if (object.IsBox()) {
    maximum_mask_region = ToLayoutBox(object).BorderBoxRect();
  } else {
    // For inline elements, depends on the value of box-decoration-break
    // there could be one box in multiple fragments or multiple boxes.
    // Either way here we are only interested in the bounding box of them.
    DCHECK(object.IsLayoutInline());
    maximum_mask_region = ToLayoutInline(object).LinesBoundingBox();
    if (object.HasFlippedBlocksWritingMode())
      object.ContainingBlock()->FlipForWritingMode(maximum_mask_region);
  }
  if (style.HasMaskBoxImageOutsets())
    maximum_mask_region.Expand(style.MaskBoxImageOutsets());
  maximum_mask_region.MoveBy(paint_offset);
  return PixelSnappedIntRect(maximum_mask_region);
}

ColorFilter CSSMaskPainter::MaskColorFilter(const LayoutObject& object) {
  if (!object.IsSVGChild())
    return kColorFilterNone;

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object);
  LayoutSVGResourceMasker* masker = resources ? resources->Masker() : nullptr;
  if (!masker)
    return kColorFilterNone;
  return masker->Style()->SvgStyle().MaskType() == MT_LUMINANCE
             ? kColorFilterLuminanceToAlpha
             : kColorFilterNone;
}

}  // namespace blink
