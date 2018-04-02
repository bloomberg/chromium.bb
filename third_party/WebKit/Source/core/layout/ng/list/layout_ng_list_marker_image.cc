// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/list/layout_ng_list_marker_image.h"
#include "core/svg/graphics/SVGImage.h"

namespace blink {

LayoutNGListMarkerImage::LayoutNGListMarkerImage(Element* element)
    : LayoutImage(element) {}

LayoutNGListMarkerImage* LayoutNGListMarkerImage::CreateAnonymous(
    Document* document) {
  LayoutNGListMarkerImage* object = new LayoutNGListMarkerImage(nullptr);
  object->SetDocumentForAnonymous(document);
  return object;
}

bool LayoutNGListMarkerImage::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectNGListMarkerImage || LayoutImage::IsOfType(type);
}

// Use default_size(ascent/2) to compute ConcreteObjectSize as svg's intrinsic
// size. Otherwise the width of svg marker will be out of control.
void LayoutNGListMarkerImage::ComputeSVGIntrinsicSizingInfoByDefaultSize(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  DCHECK(CachedImage());
  Image* image = CachedImage()->GetImage();
  DCHECK(image && image->IsSVGImage());

  const SimpleFontData* font_data = Style()->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return;

  LayoutUnit bullet_width =
      font_data->GetFontMetrics().Ascent() / LayoutUnit(2);
  FloatSize default_size(bullet_width, bullet_width);
  default_size.Scale(1 / Style()->EffectiveZoom());
  LayoutSize svg_image_size =
      RoundedLayoutSize(ToSVGImage(image)->ConcreteObjectSize(default_size));

  intrinsic_sizing_info.size.SetWidth(svg_image_size.Width());
  intrinsic_sizing_info.size.SetHeight(svg_image_size.Height());
  intrinsic_sizing_info.has_width = true;
  intrinsic_sizing_info.has_height = true;
}

bool LayoutNGListMarkerImage::GetNestedIntrinsicSizingInfo(
    IntrinsicSizingInfo& intrinsic_sizing_info) const {
  if (!LayoutImage::GetNestedIntrinsicSizingInfo(intrinsic_sizing_info))
    return false;

  // For SVG image, if GetNestedIntrinsicSizingInfo successfully and the size is
  // empty, we need to compute the intrinsic size by setting a default size.
  if (intrinsic_sizing_info.size.IsEmpty() && CachedImage()) {
    Image* image = CachedImage()->GetImage();
    if (image && image->IsSVGImage())
      ComputeSVGIntrinsicSizingInfoByDefaultSize(intrinsic_sizing_info);
  }
  return true;
}

}  // namespace blink
