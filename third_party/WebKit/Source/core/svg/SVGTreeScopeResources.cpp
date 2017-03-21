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

SVGTreeScopeResources::SVGTreeScopeResources(TreeScope* treeScope)
    : m_treeScope(treeScope) {}

SVGTreeScopeResources::~SVGTreeScopeResources() = default;

static LayoutSVGResourceContainer* lookupResource(TreeScope& treeScope,
                                                  const AtomicString& id) {
  Element* element = treeScope.getElementById(id);
  if (!element)
    return nullptr;
  LayoutObject* layoutObject = element->layoutObject();
  if (!layoutObject || !layoutObject->isSVGResourceContainer())
    return nullptr;
  return toLayoutSVGResourceContainer(layoutObject);
}

void SVGTreeScopeResources::updateResource(
    const AtomicString& id,
    LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (resource->isRegistered() || id.isEmpty())
    return;
  // Lookup the current resource. (Could differ from what's in the map if an
  // element was just added/removed.)
  LayoutSVGResourceContainer* currentResource =
      lookupResource(*m_treeScope, id);
  // Lookup the currently registered resource.
  auto it = m_resources.find(id);
  if (it != m_resources.end()) {
    // Is the local map up-to-date already?
    if (it->value == currentResource)
      return;
    unregisterResource(it);
  }
  if (currentResource)
    registerResource(id, currentResource);
}

void SVGTreeScopeResources::updateResource(
    const AtomicString& oldId,
    const AtomicString& newId,
    LayoutSVGResourceContainer* resource) {
  removeResource(oldId, resource);
  updateResource(newId, resource);
}

void SVGTreeScopeResources::removeResource(
    const AtomicString& id,
    LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (!resource->isRegistered() || id.isEmpty())
    return;
  auto it = m_resources.find(id);
  // If this is not the currently registered resource for this id, then do
  // nothing.
  if (it == m_resources.end() || it->value != resource)
    return;
  unregisterResource(it);
  // If the layout tree is being torn down, then don't attempt to update the
  // map, since that layout object is likely to be stale already.
  if (resource->documentBeingDestroyed())
    return;
  // Another resource could now be current. Perform a lookup and potentially
  // update the map.
  LayoutSVGResourceContainer* currentResource =
      lookupResource(*m_treeScope, id);
  if (!currentResource)
    return;
  // Since this is a removal, don't allow re-adding the resource.
  if (currentResource == resource)
    return;
  registerResource(id, currentResource);
}

void SVGTreeScopeResources::registerResource(
    const AtomicString& id,
    LayoutSVGResourceContainer* resource) {
  DCHECK(!id.isEmpty());
  DCHECK(resource);
  DCHECK(!resource->isRegistered());

  m_resources.set(id, resource);
  resource->setRegistered(true);

  notifyPendingClients(id);
}

void SVGTreeScopeResources::unregisterResource(ResourceMap::iterator it) {
  LayoutSVGResourceContainer* resource = it->value;
  DCHECK(resource);
  DCHECK(resource->isRegistered());

  resource->detachAllClients(it->key);

  resource->setRegistered(false);
  m_resources.erase(it);
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

void SVGTreeScopeResources::notifyPendingClients(const AtomicString& id) {
  DCHECK(!id.isEmpty());
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
  visitor->trace(m_treeScope);
}
}
