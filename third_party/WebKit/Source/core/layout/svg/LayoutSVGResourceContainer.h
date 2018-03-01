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

#ifndef LayoutSVGResourceContainer_h
#define LayoutSVGResourceContainer_h

#include "core/layout/svg/LayoutSVGHiddenContainer.h"

namespace blink {

class SVGElementProxySet;
class SVGResource;

enum LayoutSVGResourceType {
  kMaskerResourceType,
  kMarkerResourceType,
  kPatternResourceType,
  kLinearGradientResourceType,
  kRadialGradientResourceType,
  kFilterResourceType,
  kClipperResourceType
};

class LayoutSVGResourceContainer : public LayoutSVGHiddenContainer {
 public:
  explicit LayoutSVGResourceContainer(SVGElement*);
  ~LayoutSVGResourceContainer() override;

  virtual void RemoveAllClientsFromCache(bool mark_for_invalidation = true) = 0;

  // Remove any cached data for the |client|, and return true if so.
  virtual bool RemoveClientFromCache(LayoutObject& client) { return false; }

  void UpdateLayout() override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) final;
  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGResourceContainer ||
           LayoutSVGHiddenContainer::IsOfType(type);
  }

  virtual LayoutSVGResourceType ResourceType() const = 0;

  bool IsSVGPaintServer() const {
    LayoutSVGResourceType resource_type = ResourceType();
    return resource_type == kPatternResourceType ||
           resource_type == kLinearGradientResourceType ||
           resource_type == kRadialGradientResourceType;
  }

  // Detach all clients from this resource, and add them as watches to the tree
  // scope's resource entry (the argument.)
  void MakeClientsPending(SVGResource&);
  bool HasClients() const { return !clients_.IsEmpty(); }

  void InvalidateCacheAndMarkForLayout(LayoutInvalidationReasonForTracing,
                                       SubtreeLayoutScope* = nullptr);
  void InvalidateCacheAndMarkForLayout(SubtreeLayoutScope* = nullptr);

  static void MarkForLayoutAndParentResourceInvalidation(
      LayoutObject&,
      bool needs_layout = true);

  // When adding modes, make sure we don't overflow m_invalidationMask below.
  enum InvalidationMode {
    kLayoutAndBoundariesInvalidation = 1 << 0,
    kBoundariesInvalidation = 1 << 1,
    kPaintInvalidation = 1 << 2,
    kParentOnlyInvalidation = 1 << 3
  };
  static void MarkClientForInvalidation(LayoutObject&,
                                        unsigned invalidation_mask);

  void ClearInvalidationMask() { invalidation_mask_ = 0; }

 protected:
  // Used from the invalidateClient/invalidateClients methods from classes,
  // inheriting from us.
  void MarkAllClientsForInvalidation(InvalidationMode);

  void NotifyContentChanged();
  SVGElementProxySet* ElementProxySet();

  void WillBeDestroyed() override;

  bool is_in_layout_;

 private:
  friend class SVGResourcesCache;
  void AddClient(LayoutObject&);
  bool RemoveClient(LayoutObject&);

  // Track global (markAllClientsForInvalidation) invalidations to avoid
  // redundant crawls.
  unsigned invalidation_mask_ : 8;

  unsigned is_invalidating_ : 1;
  // 23 padding bits available

  HashSet<LayoutObject*> clients_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGResourceContainer,
                                IsSVGResourceContainer());

#define DEFINE_LAYOUT_SVG_RESOURCE_TYPE_CASTS(thisType, typeName)   \
  DEFINE_TYPE_CASTS(thisType, LayoutSVGResourceContainer, resource, \
                    resource->ResourceType() == typeName,           \
                    resource.ResourceType() == typeName)

}  // namespace blink

#endif
