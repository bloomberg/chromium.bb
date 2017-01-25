// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/svg/SVGTreeScopeResources.h"

#include "core/dom/Element.h"
#include "core/dom/TreeScope.h"
#include "core/layout/svg/LayoutSVGResourceContainer.h"
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

void SVGTreeScopeResources::addResource(const AtomicString& id,
                                        LayoutSVGResourceContainer* resource) {
  DCHECK(resource);
  if (id.isEmpty())
    return;
  // Replaces resource if already present, to handle potential id changes
  m_resources.set(id, resource);
}

void SVGTreeScopeResources::removeResource(const AtomicString& id) {
  if (id.isEmpty())
    return;
  m_resources.remove(id);
}

LayoutSVGResourceContainer* SVGTreeScopeResources::resourceById(
    const AtomicString& id) const {
  if (id.isEmpty())
    return nullptr;
  return m_resources.get(id);
}

void SVGTreeScopeResources::addPendingResource(const AtomicString& id,
                                               Element* element) {
  DCHECK(element);
  DCHECK(element->isConnected());

  if (id.isEmpty())
    return;

  HeapHashMap<AtomicString, Member<SVGPendingElements>>::AddResult result =
      m_pendingResources.add(id, nullptr);
  if (result.isNewEntry)
    result.storedValue->value = new SVGPendingElements;
  result.storedValue->value->add(element);

  element->setHasPendingResources();
}

bool SVGTreeScopeResources::hasPendingResource(const AtomicString& id) const {
  if (id.isEmpty())
    return false;
  return m_pendingResources.contains(id);
}

bool SVGTreeScopeResources::isElementPendingResources(Element* element) const {
  // This algorithm takes time proportional to the number of pending resources
  // and need not.
  // If performance becomes an issue we can keep a counted set of elements and
  // answer the question efficiently.
  DCHECK(element);

  for (const auto& entry : m_pendingResources) {
    SVGPendingElements* elements = entry.value.get();
    DCHECK(elements);
    if (elements->contains(element))
      return true;
  }
  return false;
}

bool SVGTreeScopeResources::isElementPendingResource(
    Element* element,
    const AtomicString& id) const {
  DCHECK(element);
  if (!hasPendingResource(id))
    return false;
  return m_pendingResources.get(id)->contains(element);
}

void SVGTreeScopeResources::clearHasPendingResourcesIfPossible(
    Element* element) {
  if (!isElementPendingResources(element))
    element->clearHasPendingResources();
}

void SVGTreeScopeResources::removeElementFromPendingResources(
    Element* element) {
  DCHECK(element);

  // Remove the element from pending resources.
  if (!m_pendingResources.isEmpty() && element->hasPendingResources()) {
    Vector<AtomicString> toBeRemoved;
    for (const auto& entry : m_pendingResources) {
      SVGPendingElements* elements = entry.value.get();
      DCHECK(elements);
      DCHECK(!elements->isEmpty());

      elements->remove(element);
      if (elements->isEmpty())
        toBeRemoved.push_back(entry.key);
    }

    clearHasPendingResourcesIfPossible(element);

    // We use the removePendingResource function here because it deals with set
    // lifetime correctly.
    for (const AtomicString& id : toBeRemoved)
      removePendingResource(id);
  }
}

SVGTreeScopeResources::SVGPendingElements*
SVGTreeScopeResources::removePendingResource(const AtomicString& id) {
  DCHECK(m_pendingResources.contains(id));
  return m_pendingResources.take(id);
}

DEFINE_TRACE(SVGTreeScopeResources) {
  visitor->trace(m_pendingResources);
}
}
