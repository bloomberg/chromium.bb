/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/dom/CustomElementUpgradeCandidateMap.h"

#include "core/dom/Element.h"

namespace WebCore {

CustomElementUpgradeCandidateMap::~CustomElementUpgradeCandidateMap()
{
    UpgradeCandidateMap::const_iterator::Keys end = m_upgradeCandidates.end().keys();
    for (UpgradeCandidateMap::const_iterator::Keys it = m_upgradeCandidates.begin().keys(); it != end; ++it)
        unregisterForElementDestructionNotification(*it, this);
}

void CustomElementUpgradeCandidateMap::add(const CustomElementDescriptor& descriptor, Element* element)
{
    element->setCustomElementState(Element::UpgradeCandidate);

    registerForElementDestructionNotification(element, this);

    UpgradeCandidateMap::AddResult result = m_upgradeCandidates.add(element, descriptor);
    ASSERT(result.isNewEntry);

    UnresolvedDefinitionMap::iterator it = m_unresolvedDefinitions.find(descriptor);
    if (it == m_unresolvedDefinitions.end())
        it = m_unresolvedDefinitions.add(descriptor, ElementSet()).iterator;
    it->value.add(element);
}

void CustomElementUpgradeCandidateMap::remove(Element* element)
{
    unregisterForElementDestructionNotification(element, this);

    UpgradeCandidateMap::iterator candidate = m_upgradeCandidates.find(element);
    ASSERT(candidate != m_upgradeCandidates.end());

    UnresolvedDefinitionMap::iterator elements = m_unresolvedDefinitions.find(candidate->value);
    ASSERT(elements != m_unresolvedDefinitions.end());
    elements->value.remove(element);
    m_upgradeCandidates.remove(candidate);
}

ListHashSet<Element*> CustomElementUpgradeCandidateMap::takeUpgradeCandidatesFor(const CustomElementDescriptor& descriptor)
{
    const ListHashSet<Element*>& candidates = m_unresolvedDefinitions.take(descriptor);

    for (ElementSet::const_iterator candidate = candidates.begin(); candidate != candidates.end(); ++candidate) {
        unregisterForElementDestructionNotification(*candidate, this);
        m_upgradeCandidates.remove(*candidate);
    }

    return candidates;
}

void CustomElementUpgradeCandidateMap::elementWasDestroyed(Element* element)
{
    DestructionObserverMap::iterator it = destructionObservers().find(element);
    if (it == destructionObservers().end())
        return;
    it->value->remove(element); // will also remove the destruction observer
}

CustomElementUpgradeCandidateMap::DestructionObserverMap& CustomElementUpgradeCandidateMap::destructionObservers()
{
    DEFINE_STATIC_LOCAL(DestructionObserverMap, map, ());
    return map;
}

void CustomElementUpgradeCandidateMap::registerForElementDestructionNotification(Element* element, CustomElementUpgradeCandidateMap* observer)
{
    DestructionObserverMap::AddResult result = destructionObservers().add(element, observer);
    ASSERT(result.isNewEntry);
}

void CustomElementUpgradeCandidateMap::unregisterForElementDestructionNotification(Element* element, CustomElementUpgradeCandidateMap* observer)
{
    CustomElementUpgradeCandidateMap* map = destructionObservers().take(element);
    ASSERT(map == observer);
}

}
