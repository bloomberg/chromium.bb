/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGResources_h
#define SVGResources_h

#include <memory>
#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

class ComputedStyle;
class LayoutObject;
class LayoutSVGResourceClipper;
class LayoutSVGResourceContainer;
class LayoutSVGResourceFilter;
class LayoutSVGResourceMarker;
class LayoutSVGResourceMasker;
class LayoutSVGResourcePaintServer;
class SVGElement;

// Holds a set of resources associated with a LayoutObject
class SVGResources {
  WTF_MAKE_NONCOPYABLE(SVGResources);
  USING_FAST_MALLOC(SVGResources);

 public:
  SVGResources();

  static std::unique_ptr<SVGResources> BuildResources(const LayoutObject*,
                                                      const ComputedStyle&);
  void LayoutIfNeeded();

  static bool SupportsMarkers(const SVGElement&);

  // Ordinary resources
  LayoutSVGResourceClipper* Clipper() const {
    return clipper_filter_masker_data_ ? clipper_filter_masker_data_->clipper
                                       : nullptr;
  }
  LayoutSVGResourceMarker* MarkerStart() const {
    return marker_data_ ? marker_data_->marker_start : nullptr;
  }
  LayoutSVGResourceMarker* MarkerMid() const {
    return marker_data_ ? marker_data_->marker_mid : nullptr;
  }
  LayoutSVGResourceMarker* MarkerEnd() const {
    return marker_data_ ? marker_data_->marker_end : nullptr;
  }
  LayoutSVGResourceMasker* Masker() const {
    return clipper_filter_masker_data_ ? clipper_filter_masker_data_->masker
                                       : nullptr;
  }

  LayoutSVGResourceFilter* Filter() const {
    if (clipper_filter_masker_data_)
      return clipper_filter_masker_data_->filter;
    return nullptr;
  }

  // Paint servers
  LayoutSVGResourcePaintServer* Fill() const {
    return fill_stroke_data_ ? fill_stroke_data_->fill : nullptr;
  }
  LayoutSVGResourcePaintServer* Stroke() const {
    return fill_stroke_data_ ? fill_stroke_data_->stroke : nullptr;
  }

  // Chainable resources - linked through xlink:href
  LayoutSVGResourceContainer* LinkedResource() const {
    return linked_resource_;
  }

  void BuildSetOfResources(HashSet<LayoutSVGResourceContainer*>&);

  // Methods operating on all cached resources
  void RemoveClientFromCache(LayoutObject*,
                             bool mark_for_invalidation = true) const;
  void RemoveClientFromCacheAffectingObjectBounds(
      LayoutObject*,
      bool mark_for_invalidation = true) const;
  void ResourceDestroyed(LayoutSVGResourceContainer*);
  void ClearReferencesTo(LayoutSVGResourceContainer*);

#ifndef NDEBUG
  void Dump(const LayoutObject*);
#endif

 private:
  bool HasResourceData() const;

  void SetClipper(LayoutSVGResourceClipper*);
  void SetFilter(LayoutSVGResourceFilter*);
  void SetMarkerStart(LayoutSVGResourceMarker*);
  void SetMarkerMid(LayoutSVGResourceMarker*);
  void SetMarkerEnd(LayoutSVGResourceMarker*);
  void SetMasker(LayoutSVGResourceMasker*);
  void SetFill(LayoutSVGResourcePaintServer*);
  void SetStroke(LayoutSVGResourcePaintServer*);
  void SetLinkedResource(LayoutSVGResourceContainer*);

  // From SVG 1.1 2nd Edition
  // clipper: 'container elements' and 'graphics elements'
  // filter:  'container elements' and 'graphics elements'
  // masker:  'container elements' and 'graphics elements'
  // -> a, circle, defs, ellipse, glyph, g, image, line, marker, mask,
  // missing-glyph, path, pattern, polygon, polyline, rect, svg, switch, symbol,
  // text, use
  struct ClipperFilterMaskerData {
    USING_FAST_MALLOC(ClipperFilterMaskerData);

   public:
    ClipperFilterMaskerData()
        : clipper(nullptr), filter(nullptr), masker(nullptr) {}

    static std::unique_ptr<ClipperFilterMaskerData> Create() {
      return WTF::WrapUnique(new ClipperFilterMaskerData);
    }

    LayoutSVGResourceClipper* clipper;
    LayoutSVGResourceFilter* filter;
    LayoutSVGResourceMasker* masker;
  };

  // From SVG 1.1 2nd Edition
  // marker: line, path, polygon, polyline
  struct MarkerData {
    USING_FAST_MALLOC(MarkerData);

   public:
    MarkerData()
        : marker_start(nullptr), marker_mid(nullptr), marker_end(nullptr) {}

    static std::unique_ptr<MarkerData> Create() {
      return WTF::WrapUnique(new MarkerData);
    }

    LayoutSVGResourceMarker* marker_start;
    LayoutSVGResourceMarker* marker_mid;
    LayoutSVGResourceMarker* marker_end;
  };

  // From SVG 1.1 2nd Edition
  // fill:       'shapes' and 'text content elements'
  // stroke:     'shapes' and 'text content elements'
  // -> circle, ellipse, line, path, polygon, polyline, rect, text, textPath,
  // tspan
  struct FillStrokeData {
    USING_FAST_MALLOC(FillStrokeData);

   public:
    FillStrokeData() : fill(nullptr), stroke(nullptr) {}

    static std::unique_ptr<FillStrokeData> Create() {
      return WTF::WrapUnique(new FillStrokeData);
    }

    LayoutSVGResourcePaintServer* fill;
    LayoutSVGResourcePaintServer* stroke;
  };

  std::unique_ptr<ClipperFilterMaskerData> clipper_filter_masker_data_;
  std::unique_ptr<MarkerData> marker_data_;
  std::unique_ptr<FillStrokeData> fill_stroke_data_;
  LayoutSVGResourceContainer* linked_resource_;
};

}  // namespace blink

#endif
