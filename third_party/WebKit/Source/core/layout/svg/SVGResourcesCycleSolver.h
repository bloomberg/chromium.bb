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

#ifndef SVGResourcesCycleSolver_h
#define SVGResourcesCycleSolver_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class LayoutObject;
class LayoutSVGResourceContainer;
class SVGResources;

class SVGResourcesCycleSolver {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(SVGResourcesCycleSolver);

 public:
  SVGResourcesCycleSolver(LayoutObject*, SVGResources*);
  ~SVGResourcesCycleSolver();

  void ResolveCycles();

  typedef HashSet<LayoutSVGResourceContainer*> ResourceSet;

 private:
  bool ResourceContainsCycles(LayoutSVGResourceContainer*);

  LayoutObject* layout_object_;
  SVGResources* resources_;

  ResourceSet active_resources_;
  ResourceSet dag_cache_;
};

}  // namespace blink

#endif
