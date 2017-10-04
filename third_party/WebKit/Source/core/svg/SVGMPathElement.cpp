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

#include "core/svg/SVGMPathElement.h"

#include "core/dom/Document.h"
#include "core/dom/IdTargetObserver.h"
#include "core/svg/SVGAnimateMotionElement.h"
#include "core/svg/SVGPathElement.h"
#include "core/svg_names.h"

namespace blink {

inline SVGMPathElement::SVGMPathElement(Document& document)
    : SVGElement(SVGNames::mpathTag, document), SVGURIReference(this) {
  DCHECK(RuntimeEnabledFeatures::SMILEnabled());
}

DEFINE_TRACE(SVGMPathElement) {
  visitor->Trace(target_id_observer_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

DEFINE_NODE_FACTORY(SVGMPathElement)

SVGMPathElement::~SVGMPathElement() {}

void SVGMPathElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;
  Element* target = ObserveTarget(target_id_observer_, *this);
  if (auto* path = ToSVGPathElementOrNull(target)) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(path);
  }
  TargetPathChanged();
}

void SVGMPathElement::ClearResourceReferences() {
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

Node::InsertionNotificationRequest SVGMPathElement::InsertedInto(
    ContainerNode* root_parent) {
  SVGElement::InsertedInto(root_parent);
  if (root_parent->isConnected())
    BuildPendingResource();
  return kInsertionDone;
}

void SVGMPathElement::RemovedFrom(ContainerNode* root_parent) {
  SVGElement::RemovedFrom(root_parent);
  NotifyParentOfPathChange(root_parent);
  if (root_parent->isConnected())
    ClearResourceReferences();
}

void SVGMPathElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    BuildPendingResource();
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

SVGPathElement* SVGMPathElement::PathElement() {
  Element* target = TargetElementFromIRIString(HrefString(), GetTreeScope());
  return ToSVGPathElementOrNull(target);
}

void SVGMPathElement::TargetPathChanged() {
  NotifyParentOfPathChange(parentNode());
}

void SVGMPathElement::NotifyParentOfPathChange(ContainerNode* parent) {
  if (auto* motion = ToSVGAnimateMotionElementOrNull(parent))
    motion->UpdateAnimationPath();
}

}  // namespace blink
