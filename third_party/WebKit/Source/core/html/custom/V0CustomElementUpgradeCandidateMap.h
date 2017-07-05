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

#ifndef V0CustomElementUpgradeCandidateMap_h
#define V0CustomElementUpgradeCandidateMap_h

#include "core/html/custom/V0CustomElementDescriptor.h"
#include "core/html/custom/V0CustomElementDescriptorHash.h"
#include "core/html/custom/V0CustomElementObserver.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/LinkedHashSet.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class V0CustomElementUpgradeCandidateMap final
    : public V0CustomElementObserver {
  WTF_MAKE_NONCOPYABLE(V0CustomElementUpgradeCandidateMap);

 public:
  static V0CustomElementUpgradeCandidateMap* Create();
  ~V0CustomElementUpgradeCandidateMap() override;

  // API for V0CustomElementRegistrationContext to save and take candidates

  typedef HeapLinkedHashSet<WeakMember<Element>> ElementSet;

  void Add(const V0CustomElementDescriptor&, Element*);
  ElementSet* TakeUpgradeCandidatesFor(const V0CustomElementDescriptor&);

  DECLARE_VIRTUAL_TRACE();

 private:
  V0CustomElementUpgradeCandidateMap() {}

  void ElementWasDestroyed(Element*) override;

  typedef HeapHashMap<WeakMember<Element>, V0CustomElementDescriptor>
      UpgradeCandidateMap;
  UpgradeCandidateMap upgrade_candidates_;

  typedef HeapHashMap<V0CustomElementDescriptor, Member<ElementSet>>
      UnresolvedDefinitionMap;
  UnresolvedDefinitionMap unresolved_definitions_;
};

}  // namespace blink

#endif  // V0CustomElementUpgradeCandidateMap_h
