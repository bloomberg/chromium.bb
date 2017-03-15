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

#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/dom/IdTargetObserver.h"
#include "core/svg/SVGAnimateMotionElement.h"
#include "core/svg/SVGPathElement.h"

namespace blink {

inline SVGMPathElement::SVGMPathElement(Document& document)
    : SVGElement(SVGNames::mpathTag, document), SVGURIReference(this) {
  DCHECK(RuntimeEnabledFeatures::smilEnabled());
}

DEFINE_TRACE(SVGMPathElement) {
  visitor->trace(m_targetIdObserver);
  SVGElement::trace(visitor);
  SVGURIReference::trace(visitor);
}

DEFINE_NODE_FACTORY(SVGMPathElement)

SVGMPathElement::~SVGMPathElement() {}

void SVGMPathElement::buildPendingResource() {
  clearResourceReferences();
  if (!isConnected())
    return;
  Element* target = observeTarget(m_targetIdObserver, *this);
  if (isSVGPathElement(target)) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    addReferenceTo(toSVGElement(target));
  }
  targetPathChanged();
}

void SVGMPathElement::clearResourceReferences() {
  unobserveTarget(m_targetIdObserver);
  removeAllOutgoingReferences();
}

Node::InsertionNotificationRequest SVGMPathElement::insertedInto(
    ContainerNode* rootParent) {
  SVGElement::insertedInto(rootParent);
  if (rootParent->isConnected())
    buildPendingResource();
  return InsertionDone;
}

void SVGMPathElement::removedFrom(ContainerNode* rootParent) {
  SVGElement::removedFrom(rootParent);
  notifyParentOfPathChange(rootParent);
  if (rootParent->isConnected())
    clearResourceReferences();
}

void SVGMPathElement::svgAttributeChanged(const QualifiedName& attrName) {
  if (SVGURIReference::isKnownAttribute(attrName)) {
    SVGElement::InvalidationGuard invalidationGuard(this);
    buildPendingResource();
    return;
  }

  SVGElement::svgAttributeChanged(attrName);
}

SVGPathElement* SVGMPathElement::pathElement() {
  Element* target = targetElementFromIRIString(hrefString(), treeScope());
  return isSVGPathElement(target) ? toSVGPathElement(target) : 0;
}

void SVGMPathElement::targetPathChanged() {
  notifyParentOfPathChange(parentNode());
}

void SVGMPathElement::notifyParentOfPathChange(ContainerNode* parent) {
  if (isSVGAnimateMotionElement(parent))
    toSVGAnimateMotionElement(parent)->updateAnimationPath();
}

}  // namespace blink
