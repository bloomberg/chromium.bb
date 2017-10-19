/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#ifndef SVGMPathElement_h
#define SVGMPathElement_h

#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"

namespace blink {

class SVGPathElement;

class SVGMPathElement final : public SVGElement, public SVGURIReference {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(SVGMPathElement);

 public:
  DECLARE_NODE_FACTORY(SVGMPathElement);

  ~SVGMPathElement() override;

  SVGPathElement* PathElement();

  void TargetPathChanged();

  virtual void Trace(blink::Visitor*);

 private:
  explicit SVGMPathElement(Document&);

  void BuildPendingResource() override;
  void ClearResourceReferences();
  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  void SvgAttributeChanged(const QualifiedName&) override;

  bool LayoutObjectIsNeeded(const ComputedStyle&) override { return false; }
  void NotifyParentOfPathChange(ContainerNode*);

  Member<IdTargetObserver> target_id_observer_;
};

}  // namespace blink

#endif  // SVGMPathElement_h
