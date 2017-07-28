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

#include "core/style/SVGComputedStyleDefs.h"

#include "core/style/DataEquivalency.h"
#include "core/style/SVGComputedStyle.h"

namespace blink {

StyleFillData::StyleFillData()
    : opacity(SVGComputedStyle::InitialFillOpacity()),
      paint_type(SVGComputedStyle::InitialFillPaintType()),
      paint_color(SVGComputedStyle::InitialFillPaintColor()),
      paint_uri(SVGComputedStyle::InitialFillPaintUri()),
      visited_link_paint_type(SVGComputedStyle::InitialStrokePaintType()),
      visited_link_paint_color(SVGComputedStyle::InitialFillPaintColor()),
      visited_link_paint_uri(SVGComputedStyle::InitialFillPaintUri()) {}

StyleFillData::StyleFillData(const StyleFillData& other)
    : RefCounted<StyleFillData>(),
      opacity(other.opacity),
      paint_type(other.paint_type),
      paint_color(other.paint_color),
      paint_uri(other.paint_uri),
      visited_link_paint_type(other.visited_link_paint_type),
      visited_link_paint_color(other.visited_link_paint_color),
      visited_link_paint_uri(other.visited_link_paint_uri) {}

bool StyleFillData::operator==(const StyleFillData& other) const {
  return opacity == other.opacity && paint_type == other.paint_type &&
         paint_color == other.paint_color && paint_uri == other.paint_uri &&
         visited_link_paint_type == other.visited_link_paint_type &&
         visited_link_paint_color == other.visited_link_paint_color &&
         visited_link_paint_uri == other.visited_link_paint_uri;
}

StyleStrokeData::StyleStrokeData()
    : opacity(SVGComputedStyle::InitialStrokeOpacity()),
      miter_limit(SVGComputedStyle::InitialStrokeMiterLimit()),
      width(SVGComputedStyle::InitialStrokeWidth()),
      dash_offset(SVGComputedStyle::InitialStrokeDashOffset()),
      dash_array(SVGComputedStyle::InitialStrokeDashArray()),
      paint_type(SVGComputedStyle::InitialStrokePaintType()),
      paint_color(SVGComputedStyle::InitialStrokePaintColor()),
      paint_uri(SVGComputedStyle::InitialStrokePaintUri()),
      visited_link_paint_type(SVGComputedStyle::InitialStrokePaintType()),
      visited_link_paint_color(SVGComputedStyle::InitialStrokePaintColor()),
      visited_link_paint_uri(SVGComputedStyle::InitialStrokePaintUri()) {}

StyleStrokeData::StyleStrokeData(const StyleStrokeData& other)
    : RefCounted<StyleStrokeData>(),
      opacity(other.opacity),
      miter_limit(other.miter_limit),
      width(other.width),
      dash_offset(other.dash_offset),
      dash_array(other.dash_array),
      paint_type(other.paint_type),
      paint_color(other.paint_color),
      paint_uri(other.paint_uri),
      visited_link_paint_type(other.visited_link_paint_type),
      visited_link_paint_color(other.visited_link_paint_color),
      visited_link_paint_uri(other.visited_link_paint_uri) {}

bool StyleStrokeData::operator==(const StyleStrokeData& other) const {
  return width == other.width && opacity == other.opacity &&
         miter_limit == other.miter_limit && dash_offset == other.dash_offset &&
         *dash_array == *other.dash_array && paint_type == other.paint_type &&
         paint_color == other.paint_color && paint_uri == other.paint_uri &&
         visited_link_paint_type == other.visited_link_paint_type &&
         visited_link_paint_color == other.visited_link_paint_color &&
         visited_link_paint_uri == other.visited_link_paint_uri;
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

PassRefPtr<StyleGeometryData> StyleGeometryData::Copy() const {
  return AdoptRef(new StyleGeometryData(*this));
}

bool StyleGeometryData::operator==(const StyleGeometryData& other) const {
  return x == other.x && y == other.y && r == other.r && rx == other.rx &&
         ry == other.ry && cx == other.cx && cy == other.cy &&
         DataEquivalent(d, other.d);
}

}  // namespace blink
