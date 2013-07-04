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

namespace WebCore {

void CustomElementUpgradeCandidateMap::add(CustomElementDefinition::CustomElementKind kind, const AtomicString& type, Element* element)
{
    m_upgradeCandidates.add(element, RequiredDefinition(kind, type));

    UnresolvedDefinitionMap::iterator it = m_unresolvedDefinitions.find(type);
    if (it == m_unresolvedDefinitions.end())
        it = m_unresolvedDefinitions.add(type, ElementSet()).iterator;
    it->value.add(element);
}

void CustomElementUpgradeCandidateMap::remove(Element* element)
{
    UpgradeCandidateMap::iterator it = m_upgradeCandidates.find(element);
    if (it == m_upgradeCandidates.end())
        return;

    const AtomicString& type = it->value.second;
    m_unresolvedDefinitions.get(type).remove(element);
    m_upgradeCandidates.remove(it);
}

Vector<Element*> CustomElementUpgradeCandidateMap::takeUpgradeCandidatesFor(CustomElementDefinition* definition)
{
    UnresolvedDefinitionMap::iterator it = m_unresolvedDefinitions.find(definition->type());
    if (it == m_unresolvedDefinitions.end())
        return Vector<Element*>();

    const ElementSet& candidatesForThisType = it->value;
    Vector<Element*> matchingCandidates;

    // Filter the set based on whether the definition matches
    for (ElementSet::const_iterator candidate = candidatesForThisType.begin(); candidate != candidatesForThisType.end(); ++candidate) {
        if (matches(definition, *candidate)) {
            matchingCandidates.append(*candidate);
            m_upgradeCandidates.remove(*candidate);
        }
    }

    m_unresolvedDefinitions.remove(it);
    return matchingCandidates;
}

bool CustomElementUpgradeCandidateMap::matches(CustomElementDefinition* definition, Element* element)
{
    ASSERT(m_upgradeCandidates.contains(element));
    const RequiredDefinition& requirement = m_upgradeCandidates.get(element);
    return definition->kind() == requirement.first && definition->type() == requirement.second && definition->namespaceURI() == element->namespaceURI() && definition->name() == element->localName();
}

}
