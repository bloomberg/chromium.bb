/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

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

#ifndef SVGComputedStyleDefs_h
#define SVGComputedStyleDefs_h

#include "core/CoreExport.h"
#include "core/style/StylePath.h"
#include "platform/Length.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/RefCounted.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/RefVector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

typedef RefVector<Length> SVGDashArray;

enum SVGPaintType {
  SVG_PAINTTYPE_RGBCOLOR,
  SVG_PAINTTYPE_NONE,
  SVG_PAINTTYPE_CURRENTCOLOR,
  SVG_PAINTTYPE_URI_NONE,
  SVG_PAINTTYPE_URI_CURRENTCOLOR,
  SVG_PAINTTYPE_URI_RGBCOLOR,
  SVG_PAINTTYPE_URI
};

enum EBaselineShift { BS_LENGTH, BS_SUB, BS_SUPER };

enum ETextAnchor { TA_START, TA_MIDDLE, TA_END };

enum EColorInterpolation { CI_AUTO, CI_SRGB, CI_LINEARRGB };

enum EColorRendering { CR_AUTO, CR_OPTIMIZESPEED, CR_OPTIMIZEQUALITY };
enum EShapeRendering {
  SR_AUTO,
  SR_OPTIMIZESPEED,
  SR_CRISPEDGES,
  SR_GEOMETRICPRECISION
};

enum EAlignmentBaseline {
  AB_AUTO,
  AB_BASELINE,
  AB_BEFORE_EDGE,
  AB_TEXT_BEFORE_EDGE,
  AB_MIDDLE,
  AB_CENTRAL,
  AB_AFTER_EDGE,
  AB_TEXT_AFTER_EDGE,
  AB_IDEOGRAPHIC,
  AB_ALPHABETIC,
  AB_HANGING,
  AB_MATHEMATICAL
};

enum EDominantBaseline {
  DB_AUTO,
  DB_USE_SCRIPT,
  DB_NO_CHANGE,
  DB_RESET_SIZE,
  DB_IDEOGRAPHIC,
  DB_ALPHABETIC,
  DB_HANGING,
  DB_MATHEMATICAL,
  DB_CENTRAL,
  DB_MIDDLE,
  DB_TEXT_AFTER_EDGE,
  DB_TEXT_BEFORE_EDGE
};

enum EVectorEffect { VE_NONE, VE_NON_SCALING_STROKE };

enum EBufferedRendering { BR_AUTO, BR_DYNAMIC, BR_STATIC };

enum EMaskType { MT_LUMINANCE, MT_ALPHA };

enum EPaintOrderType {
  PT_NONE = 0,
  PT_FILL = 1,
  PT_STROKE = 2,
  PT_MARKERS = 3
};

enum EPaintOrder {
  kPaintOrderNormal = 0,
  kPaintOrderFillStrokeMarkers = 1,
  kPaintOrderFillMarkersStroke = 2,
  kPaintOrderStrokeFillMarkers = 3,
  kPaintOrderStrokeMarkersFill = 4,
  kPaintOrderMarkersFillStroke = 5,
  kPaintOrderMarkersStrokeFill = 6
};

// Inherited/Non-Inherited Style Datastructures
class StyleFillData : public RefCounted<StyleFillData> {
 public:
  static RefPtr<StyleFillData> Create() { return AdoptRef(new StyleFillData); }
  RefPtr<StyleFillData> Copy() const {
    return AdoptRef(new StyleFillData(*this));
  }

  bool operator==(const StyleFillData&) const;
  bool operator!=(const StyleFillData& other) const {
    return !(*this == other);
  }

  float opacity;
  SVGPaintType paint_type;
  Color paint_color;
  String paint_uri;
  SVGPaintType visited_link_paint_type;
  Color visited_link_paint_color;
  String visited_link_paint_uri;

 private:
  StyleFillData();
  StyleFillData(const StyleFillData&);
};

class UnzoomedLength {
  DISALLOW_NEW();

 public:
  explicit UnzoomedLength(const Length& length) : length_(length) {}

  bool IsZero() const { return length_.IsZero(); }

  bool operator==(const UnzoomedLength& other) const {
    return length_ == other.length_;
  }
  bool operator!=(const UnzoomedLength& other) const {
    return !operator==(other);
  }

  const Length& length() const { return length_; }

 private:
  Length length_;
};

class CORE_EXPORT StyleStrokeData : public RefCounted<StyleStrokeData> {
 public:
  static RefPtr<StyleStrokeData> Create() {
    return AdoptRef(new StyleStrokeData);
  }

  RefPtr<StyleStrokeData> Copy() const {
    return AdoptRef(new StyleStrokeData(*this));
  }

  bool operator==(const StyleStrokeData&) const;
  bool operator!=(const StyleStrokeData& other) const {
    return !(*this == other);
  }

  float opacity;
  float miter_limit;

  UnzoomedLength width;
  Length dash_offset;
  RefPtr<SVGDashArray> dash_array;

  SVGPaintType paint_type;
  Color paint_color;
  String paint_uri;
  SVGPaintType visited_link_paint_type;
  Color visited_link_paint_color;
  String visited_link_paint_uri;

 private:
  StyleStrokeData();
  StyleStrokeData(const StyleStrokeData&);
};

class StyleStopData : public RefCounted<StyleStopData> {
 public:
  static RefPtr<StyleStopData> Create() { return AdoptRef(new StyleStopData); }
  RefPtr<StyleStopData> Copy() const {
    return AdoptRef(new StyleStopData(*this));
  }

  bool operator==(const StyleStopData&) const;
  bool operator!=(const StyleStopData& other) const {
    return !(*this == other);
  }

  float opacity;
  Color color;

 private:
  StyleStopData();
  StyleStopData(const StyleStopData&);
};

// Note: the rule for this class is, *no inheritance* of these props
class CORE_EXPORT StyleMiscData : public RefCounted<StyleMiscData> {
 public:
  static RefPtr<StyleMiscData> Create() { return AdoptRef(new StyleMiscData); }
  RefPtr<StyleMiscData> Copy() const {
    return AdoptRef(new StyleMiscData(*this));
  }

  bool operator==(const StyleMiscData&) const;
  bool operator!=(const StyleMiscData& other) const {
    return !(*this == other);
  }

  Color flood_color;
  float flood_opacity;
  Color lighting_color;

  Length baseline_shift_value;

 private:
  StyleMiscData();
  StyleMiscData(const StyleMiscData&);
};

// Non-inherited resources
class StyleResourceData : public RefCounted<StyleResourceData> {
 public:
  static RefPtr<StyleResourceData> Create() {
    return AdoptRef(new StyleResourceData);
  }
  RefPtr<StyleResourceData> Copy() const {
    return AdoptRef(new StyleResourceData(*this));
  }

  bool operator==(const StyleResourceData&) const;
  bool operator!=(const StyleResourceData& other) const {
    return !(*this == other);
  }

  AtomicString masker;

 private:
  StyleResourceData();
  StyleResourceData(const StyleResourceData&);
};

// Inherited resources
class StyleInheritedResourceData
    : public RefCounted<StyleInheritedResourceData> {
 public:
  static RefPtr<StyleInheritedResourceData> Create() {
    return AdoptRef(new StyleInheritedResourceData);
  }
  RefPtr<StyleInheritedResourceData> Copy() const {
    return AdoptRef(new StyleInheritedResourceData(*this));
  }

  bool operator==(const StyleInheritedResourceData&) const;
  bool operator!=(const StyleInheritedResourceData& other) const {
    return !(*this == other);
  }

  AtomicString marker_start;
  AtomicString marker_mid;
  AtomicString marker_end;

 private:
  StyleInheritedResourceData();
  StyleInheritedResourceData(const StyleInheritedResourceData&);
};

// Geometry properties
class StyleGeometryData : public RefCounted<StyleGeometryData> {
 public:
  static RefPtr<StyleGeometryData> Create() {
    return AdoptRef(new StyleGeometryData);
  }
  RefPtr<StyleGeometryData> Copy() const;
  bool operator==(const StyleGeometryData&) const;
  bool operator!=(const StyleGeometryData& other) const {
    return !(*this == other);
  }
  RefPtr<StylePath> d;
  Length cx;
  Length cy;
  Length x;
  Length y;
  Length r;
  Length rx;
  Length ry;

 private:
  StyleGeometryData();
  StyleGeometryData(const StyleGeometryData&);
};

}  // namespace blink

#endif  // SVGComputedStyleDefs_h
