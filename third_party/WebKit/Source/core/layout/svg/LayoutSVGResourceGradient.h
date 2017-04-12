/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef LayoutSVGResourceGradient_h
#define LayoutSVGResourceGradient_h

#include <memory>
#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "core/svg/SVGGradientElement.h"
#include "platform/graphics/Gradient.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/HashMap.h"

namespace blink {

struct GradientData {
  USING_FAST_MALLOC(GradientData);

 public:
  RefPtr<Gradient> gradient;
  AffineTransform userspace_transform;
};

class LayoutSVGResourceGradient : public LayoutSVGResourcePaintServer {
 public:
  explicit LayoutSVGResourceGradient(SVGGradientElement*);

  void RemoveAllClientsFromCache(bool mark_for_invalidation = true) final;
  void RemoveClientFromCache(LayoutObject*,
                             bool mark_for_invalidation = true) final;

  SVGPaintServer PreparePaintServer(const LayoutObject&) final;

  bool IsChildAllowed(LayoutObject* child, const ComputedStyle&) const final;

 protected:
  virtual SVGUnitTypes::SVGUnitType GradientUnits() const = 0;
  virtual AffineTransform CalculateGradientTransform() const = 0;
  virtual bool CollectGradientAttributes() = 0;
  virtual PassRefPtr<Gradient> BuildGradient() const = 0;

  static GradientSpreadMethod PlatformSpreadMethodFromSVGType(
      SVGSpreadMethodType);

 private:
  bool should_collect_gradient_attributes_ : 1;
  HashMap<const LayoutObject*, std::unique_ptr<GradientData>> gradient_map_;
};

}  // namespace blink

#endif
