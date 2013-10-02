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

#ifndef CustomElementUpgradeCandidateMap_h
#define CustomElementUpgradeCandidateMap_h

#include "core/dom/custom/CustomElementDescriptor.h"
#include "core/dom/custom/CustomElementDescriptorHash.h"
#include "core/dom/custom/CustomElementObserver.h"
#include "wtf/HashMap.h"
#include "wtf/ListHashSet.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class Element;

class CustomElementUpgradeCandidateMap : CustomElementObserver {
    WTF_MAKE_NONCOPYABLE(CustomElementUpgradeCandidateMap);
public:
    CustomElementUpgradeCandidateMap() { }
    ~CustomElementUpgradeCandidateMap();

    // API for CustomElementRegistrationContext to save and take candidates

    typedef ListHashSet<Element*> ElementSet;

    void add(const CustomElementDescriptor&, Element*);
    void remove(Element*);
    ElementSet takeUpgradeCandidatesFor(const CustomElementDescriptor&);

private:
    virtual void elementWasDestroyed(Element*) OVERRIDE;
    void removeCommon(Element*);

    virtual void elementDidFinishParsingChildren(Element*) OVERRIDE;
    void moveToEnd(Element*);

    typedef HashMap<Element*, CustomElementDescriptor> UpgradeCandidateMap;
    UpgradeCandidateMap m_upgradeCandidates;

    typedef HashMap<CustomElementDescriptor, ElementSet> UnresolvedDefinitionMap;
    UnresolvedDefinitionMap m_unresolvedDefinitions;
};

}

#endif // CustomElementUpgradeCandidateMap_h
