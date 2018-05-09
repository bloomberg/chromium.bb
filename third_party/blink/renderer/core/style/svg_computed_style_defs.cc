/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
    Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
    Copyright (C) 2002-2003 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/style/svg_computed_style_defs.h"

#include "third_party/blink/renderer/core/style/data_equivalency.h"
#include "third_party/blink/renderer/core/style/svg_computed_style.h"

namespace blink {

bool SVGPaint::operator==(const SVGPaint& other) const {
  return type == other.type && color == other.color && url == other.url;
}

StyleFillData::StyleFillData()
    : opacity(SVGComputedStyle::InitialFillOpacity()),
      paint(SVGComputedStyle::InitialFillPaint()),
      visited_link_paint(SVGComputedStyle::InitialFillPaint()) {}

StyleFillData::StyleFillData(const StyleFillData& other)
    : RefCounted<StyleFillData>(),
      opacity(other.opacity),
      paint(other.paint),
      visited_link_paint(other.visited_link_paint) {}

bool StyleFillData::operator==(const StyleFillData& other) const {
  return opacity == other.opacity && paint == other.paint &&
         visited_link_paint == other.visited_link_paint;
}

StyleStrokeData::StyleStrokeData()
    : opacity(SVGComputedStyle::InitialStrokeOpacity()),
      miter_limit(SVGComputedStyle::InitialStrokeMiterLimit()),
      width(SVGComputedStyle::InitialStrokeWidth()),
      dash_offset(SVGComputedStyle::InitialStrokeDashOffset()),
      dash_array(SVGComputedStyle::InitialStrokeDashArray()),
      paint(SVGComputedStyle::InitialStrokePaint()),
      visited_link_paint(SVGComputedStyle::InitialStrokePaint()) {}

StyleStrokeData::StyleStrokeData(const StyleStrokeData& other)
    : RefCounted<StyleStrokeData>(),
      opacity(other.opacity),
      miter_limit(other.miter_limit),
      width(other.width),
      dash_offset(other.dash_offset),
      dash_array(other.dash_array),
      paint(other.paint),
      visited_link_paint(other.visited_link_paint) {}

bool StyleStrokeData::operator==(const StyleStrokeData& other) const {
  return width == other.width && opacity == other.opacity &&
         miter_limit == other.miter_limit && dash_offset == other.dash_offset &&
         *dash_array == *other.dash_array && paint == other.paint &&
         visited_link_paint == other.visited_link_paint;
}

StyleStopData::StyleStopData()
    : opacity(SVGComputedStyle::InitialStopOpacity()),
      color(SVGComputedStyle::InitialStopColor()) {}

StyleStopData::StyleStopData(const StyleStopData& other)
    : RefCounted<StyleStopData>(), opacity(other.opacity), color(other.color) {}

bool StyleStopData::operator==(const StyleStopData& other) const {
  return color == other.color && opacity == other.opacity;
}

StyleMiscData::StyleMiscData()
    : flood_color(SVGComputedStyle::InitialFloodColor()),
      flood_opacity(SVGComputedStyle::InitialFloodOpacity()),
      lighting_color(SVGComputedStyle::InitialLightingColor()),
      baseline_shift_value(SVGComputedStyle::InitialBaselineShiftValue()) {}

StyleMiscData::StyleMiscData(const StyleMiscData& other)
    : RefCounted<StyleMiscData>(),
      flood_color(other.flood_color),
      flood_opacity(other.flood_opacity),
      lighting_color(other.lighting_color),
      baseline_shift_value(other.baseline_shift_value) {}

bool StyleMiscData::operator==(const StyleMiscData& other) const {
  return flood_opacity == other.flood_opacity &&
         flood_color == other.flood_color &&
         lighting_color == other.lighting_color &&
         baseline_shift_value == other.baseline_shift_value;
}

StyleResourceData::StyleResourceData()
    : masker(SVGComputedStyle::InitialMaskerResource()) {}

StyleResourceData::StyleResourceData(const StyleResourceData& other)
    : RefCounted<StyleResourceData>(), masker(other.masker) {}

bool StyleResourceData::operator==(const StyleResourceData& other) const {
  return masker == other.masker;
}

StyleInheritedResourceData::StyleInheritedResourceData()
    : marker_start(SVGComputedStyle::InitialMarkerStartResource()),
      marker_mid(SVGComputedStyle::InitialMarkerMidResource()),
      marker_end(SVGComputedStyle::InitialMarkerEndResource()) {}

StyleInheritedResourceData::StyleInheritedResourceData(
    const StyleInheritedResourceData& other)
    : RefCounted<StyleInheritedResourceData>(),
      marker_start(other.marker_start),
      marker_mid(other.marker_mid),
      marker_end(other.marker_end) {}

bool StyleInheritedResourceData::operator==(
    const StyleInheritedResourceData& other) const {
  return marker_start == other.marker_start && marker_mid == other.marker_mid &&
         marker_end == other.marker_end;
}

StyleGeometryData::StyleGeometryData()
    : d(SVGComputedStyle::InitialD()),
      cx(SVGComputedStyle::InitialCx()),
      cy(SVGComputedStyle::InitialCy()),
      x(SVGComputedStyle::InitialX()),
      y(SVGComputedStyle::InitialY()),
      r(SVGComputedStyle::InitialR()),
      rx(SVGComputedStyle::InitialRx()),
      ry(SVGComputedStyle::InitialRy()) {}

inline StyleGeometryData::StyleGeometryData(const StyleGeometryData& other)
    : RefCounted<StyleGeometryData>(),
      d(other.d),
      cx(other.cx),
      cy(other.cy),
      x(other.x),
      y(other.y),
      r(other.r),
      rx(other.rx),
      ry(other.ry) {}

scoped_refptr<StyleGeometryData> StyleGeometryData::Copy() const {
  return base::AdoptRef(new StyleGeometryData(*this));
}

bool StyleGeometryData::operator==(const StyleGeometryData& other) const {
  return x == other.x && y == other.y && r == other.r && rx == other.rx &&
         ry == other.ry && cx == other.cx && cy == other.cy &&
         DataEquivalent(d, other.d);
}

}  // namespace blink
