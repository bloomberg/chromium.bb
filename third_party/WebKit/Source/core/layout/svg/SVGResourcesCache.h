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

#ifndef SVGResourcesCache_h
#define SVGResourcesCache_h

#include "core/style/StyleDifference.h"
#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class LayoutObject;
class ComputedStyle;
class LayoutSVGResourceContainer;
class SVGResources;

class SVGResourcesCache {
  WTF_MAKE_NONCOPYABLE(SVGResourcesCache);
  USING_FAST_MALLOC(SVGResourcesCache);

 public:
  SVGResourcesCache();
  ~SVGResourcesCache();

  static SVGResources* CachedResourcesForLayoutObject(const LayoutObject*);

  // Called from all SVG layoutObjects addChild() methods.
  static void ClientWasAddedToTree(LayoutObject*,
                                   const ComputedStyle& new_style);

  // Called from all SVG layoutObjects removeChild() methods.
  static void ClientWillBeRemovedFromTree(LayoutObject*);

  // Called from all SVG layoutObjects destroy() methods - except for
  // LayoutSVGResourceContainer.
  static void ClientDestroyed(LayoutObject*);

  // Called from all SVG layoutObjects layout() methods.
  static void ClientLayoutChanged(LayoutObject*);

  // Called from all SVG layoutObjects styleDidChange() methods.
  static void ClientStyleChanged(LayoutObject*,
                                 StyleDifference,
                                 const ComputedStyle& new_style);

 private:
  void AddResourcesFromLayoutObject(LayoutObject*, const ComputedStyle&);
  void RemoveResourcesFromLayoutObject(LayoutObject*);

  typedef HashMap<const LayoutObject*, std::unique_ptr<SVGResources>> CacheMap;
  CacheMap cache_;
};

}  // namespace blink

#endif
