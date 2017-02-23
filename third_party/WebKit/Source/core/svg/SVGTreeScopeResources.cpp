// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGTreeScopeResources.h"

#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/svg/SVGUseElement.h"
#include "wtf/text/AtomicString.h"

namespace blink {

SVGTreeScopeResources::SVGTreeScopeResources(TreeScope* treeScope) {
  // Whenever an object of SVGTreeScopeResources is created, to keep the code
  // behave as before,
  // the document should also have an instance of SVGDocumentExtensions created.
  // Thus below line is added.
  treeScope->document().accessSVGExtensions();
}

SVGTreeScopeResources::~SVGTreeScopeResources() = default;

void SVGTreeScopeResources::updateResource(
    const AtomicString& id,
    LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (id.isEmpty())
    return;
  // Replaces resource if already present, to handle potential id changes.
  m_resources.set(id, resource);

  SVGPendingElements* pendingElements = m_pendingResources.take(id);
  if (!pendingElements)
    return;
  // Update cached resources of pending clients.
  for (Element* clientElement : *pendingElements) {
    DCHECK(clientElement->hasPendingResources());
    clearHasPendingResourcesIfPossible(*clientElement);

    LayoutObject* layoutObject = clientElement->layoutObject();
    if (!layoutObject)
      continue;
    DCHECK(layoutObject->isSVG());

    StyleDifference diff;
    diff.setNeedsFullLayout();
    SVGResourcesCache::clientStyleChanged(layoutObject, diff,
                                          layoutObject->styleRef());
    layoutObject->setNeedsLayoutAndFullPaintInvalidation(
        LayoutInvalidationReason::SvgResourceInvalidated);
  }
}

void SVGTreeScopeResources::removeResource(const AtomicString& id) {
  if (id.isEmpty())
    return;
  m_resources.erase(id);
}

LayoutSVGResourceContainer* SVGTreeScopeResources::resourceById(
    const AtomicString& id) const {
  if (id.isEmpty())
    return nullptr;
  return m_resources.at(id);
}

void SVGTreeScopeResources::addPendingResource(const AtomicString& id,
                                               Element& element) {
  DCHECK(element.isConnected());

  if (id.isEmpty())
    return;
  auto result = m_pendingResources.insert(id, nullptr);
  if (result.isNewEntry)
    result.storedValue->value = new SVGPendingElements;
  result.storedValue->value->insert(&element);

  element.setHasPendingResources();
}

bool SVGTreeScopeResources::isElementPendingResource(
    Element& element,
    const AtomicString& id) const {
  if (id.isEmpty())
    return false;
  const SVGPendingElements* pendingElements = m_pendingResources.at(id);
  return pendingElements && pendingElements->contains(&element);
}

void SVGTreeScopeResources::clearHasPendingResourcesIfPossible(
    Element& element) {
  // This algorithm takes time proportional to the number of pending resources
  // and need not.
  // If performance becomes an issue we can keep a counted set of elements and
  // answer the question efficiently.
  for (const auto& entry : m_pendingResources) {
    SVGPendingElements* elements = entry.value.get();
    DCHECK(elements);
    if (elements->contains(&element))
      return;
  }
  element.clearHasPendingResources();
}

void SVGTreeScopeResources::removeElementFromPendingResources(
    Element& element) {
  if (m_pendingResources.isEmpty() || !element.hasPendingResources())
    return;
  // Remove the element from pending resources.
  Vector<AtomicString> toBeRemoved;
  for (const auto& entry : m_pendingResources) {
    SVGPendingElements* elements = entry.value.get();
    DCHECK(elements);
    DCHECK(!elements->isEmpty());

    elements->erase(&element);
    if (elements->isEmpty())
      toBeRemoved.push_back(entry.key);
  }
  m_pendingResources.removeAll(toBeRemoved);

  clearHasPendingResourcesIfPossible(element);
}

void SVGTreeScopeResources::notifyResourceAvailable(const AtomicString& id) {
  if (id.isEmpty())
    return;
  // Get pending elements for this id.
  SVGPendingElements* pendingElements = m_pendingResources.take(id);
  if (!pendingElements)
    return;
  // Rebuild pending resources for each client of a pending resource that is
  // being removed.
  for (Element* clientElement : *pendingElements) {
    DCHECK(clientElement->hasPendingResources());
    if (!clientElement->hasPendingResources())
      continue;
    // TODO(fs): Ideally we'd always resolve pending resources async instead of
    // inside insertedInto and svgAttributeChanged. For now we only do it for
    // <use> since that would stamp out DOM.
    if (isSVGUseElement(clientElement))
      toSVGUseElement(clientElement)->invalidateShadowTree();
    else
      clientElement->buildPendingResource();

    clearHasPendingResourcesIfPossible(*clientElement);
  }
}

DEFINE_TRACE(SVGTreeScopeResources) {
  visitor->trace(m_pendingResources);
}
}
