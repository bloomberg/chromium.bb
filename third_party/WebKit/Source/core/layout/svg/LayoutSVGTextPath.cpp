/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "core/layout/svg/LayoutSVGTextPath.h"

#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg/SVGTextPathElement.h"
#include "platform/graphics/Path.h"
#include <memory>

namespace blink {

PathPositionMapper::PathPositionMapper(const Path& path)
    : position_calculator_(path), path_length_(path.length()) {}

PathPositionMapper::PositionType PathPositionMapper::PointAndNormalAtLength(
    float length,
    FloatPoint& point,
    float& angle) {
  if (length < 0)
    return kBeforePath;
  if (length > path_length_)
    return kAfterPath;
  DCHECK_GE(length, 0);
  DCHECK_LE(length, path_length_);
  position_calculator_.PointAndNormalAtLength(length, point, angle);
  return kOnPath;
}

LayoutSVGTextPath::LayoutSVGTextPath(Element* element)
    : LayoutSVGInline(element) {}

bool LayoutSVGTextPath::IsChildAllowed(LayoutObject* child,
                                       const ComputedStyle&) const {
  if (child->IsText())
    return SVGLayoutSupport::IsLayoutableTextNode(child);

  return child->IsSVGInline() && !child->IsSVGTextPath();
}

std::unique_ptr<PathPositionMapper> LayoutSVGTextPath::LayoutPath() const {
  const SVGTextPathElement& text_path_element =
      ToSVGTextPathElement(*GetNode());
  Element* target_element = SVGURIReference::TargetElementFromIRIString(
      text_path_element.HrefString(),
      text_path_element.TreeScopeForIdResolution());

  if (!IsSVGPathElement(target_element))
    return nullptr;

  SVGPathElement& path_element = ToSVGPathElement(*target_element);
  Path path_data = path_element.AsPath();
  if (path_data.IsEmpty())
    return nullptr;

  // Spec:  The transform attribute on the referenced 'path' element represents
  // a supplemental transformation relative to the current user coordinate
  // system for the current 'text' element, including any adjustments to the
  // current user coordinate system due to a possible transform attribute on the
  // current 'text' element. http://www.w3.org/TR/SVG/text.html#TextPathElement
  path_data.Transform(
      path_element.CalculateTransform(SVGElement::kIncludeMotionTransform));

  return PathPositionMapper::Create(path_data);
}

float LayoutSVGTextPath::CalculateStartOffset(float path_length) const {
  const SVGLength& start_offset =
      *ToSVGTextPathElement(GetNode())->startOffset()->CurrentValue();
  float text_path_start_offset = start_offset.ValueAsPercentage();
  if (start_offset.TypeWithCalcResolved() ==
      CSSPrimitiveValue::UnitType::kPercentage)
    text_path_start_offset *= path_length;

  return text_path_start_offset;
}

}  // namespace blink
