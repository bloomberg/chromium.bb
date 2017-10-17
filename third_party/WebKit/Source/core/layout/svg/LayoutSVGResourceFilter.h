/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef LayoutSVGResourceFilter_h
#define LayoutSVGResourceFilter_h

#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/svg/SVGUnitTypes.h"

namespace blink {

class FilterEffect;
class SVGFilterElement;
class SVGFilterGraphNodeMap;
class SVGFilterPrimitiveStandardAttributes;

class FilterData final : public GarbageCollected<FilterData> {
 public:
  /*
   * The state transitions should follow the following:
   * Initial->RecordingContent->ReadyToPaint->PaintingFilter->ReadyToPaint
   *              |     ^                       |     ^
   *              v     |                       v     |
   *     RecordingContentCycleDetected     PaintingFilterCycle
   */
  enum FilterDataState {
    kInitial,
    kRecordingContent,
    kRecordingContentCycleDetected,
    kReadyToPaint,
    kPaintingFilter,
    kPaintingFilterCycleDetected
  };

  static FilterData* Create() { return new FilterData(); }

  void Dispose();

  DECLARE_TRACE();

  Member<FilterEffect> last_effect;
  Member<SVGFilterGraphNodeMap> node_map;
  FilterDataState state_;

 private:
  FilterData() : state_(kInitial) {}
};

class LayoutSVGResourceFilter final : public LayoutSVGResourceContainer {
 public:
  explicit LayoutSVGResourceFilter(SVGFilterElement*);
  ~LayoutSVGResourceFilter() override;

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  const char* GetName() const override { return "LayoutSVGResourceFilter"; }
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGResourceFilter ||
           LayoutSVGResourceContainer::IsOfType(type);
  }

  void RemoveAllClientsFromCache(bool mark_for_invalidation = true) override;
  void RemoveClientFromCache(LayoutObject*,
                             bool mark_for_invalidation = true) override;

  FloatRect ResourceBoundingBox(const LayoutObject*);

  SVGUnitTypes::SVGUnitType FilterUnits() const;
  SVGUnitTypes::SVGUnitType PrimitiveUnits() const;

  void PrimitiveAttributeChanged(SVGFilterPrimitiveStandardAttributes&,
                                 const QualifiedName&);

  static const LayoutSVGResourceType kResourceType = kFilterResourceType;
  LayoutSVGResourceType ResourceType() const override { return kResourceType; }

  FilterData* GetFilterDataForLayoutObject(const LayoutObject* object) {
    return filter_.at(const_cast<LayoutObject*>(object));
  }
  void SetFilterDataForLayoutObject(LayoutObject* object,
                                    FilterData* filter_data) {
    filter_.Set(object, filter_data);
  }

 protected:
  void WillBeDestroyed() override;

 private:
  void DisposeFilterMap();

  using FilterMap = PersistentHeapHashMap<LayoutObject*, Member<FilterData>>;
  FilterMap filter_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGResourceFilter, IsSVGResourceFilter());

}  // namespace blink

#endif
