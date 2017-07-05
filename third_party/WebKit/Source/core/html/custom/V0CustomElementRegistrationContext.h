/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef V0CustomElementRegistrationContext_h
#define V0CustomElementRegistrationContext_h

#include "core/dom/QualifiedName.h"
#include "core/html/custom/V0CustomElementDescriptor.h"
#include "core/html/custom/V0CustomElementRegistry.h"
#include "core/html/custom/V0CustomElementUpgradeCandidateMap.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class CustomElementRegistry;

class V0CustomElementRegistrationContext final
    : public GarbageCollectedFinalized<V0CustomElementRegistrationContext> {
 public:
  static V0CustomElementRegistrationContext* Create() {
    return new V0CustomElementRegistrationContext();
  }

  ~V0CustomElementRegistrationContext() {}
  void DocumentWasDetached() { registry_.DocumentWasDetached(); }

  // Definitions
  void RegisterElement(Document*,
                       V0CustomElementConstructorBuilder*,
                       const AtomicString& type,
                       V0CustomElement::NameSet valid_names,
                       ExceptionState&);

  Element* CreateCustomTagElement(Document&, const QualifiedName&);
  static void SetIsAttributeAndTypeExtension(Element*,
                                             const AtomicString& type);
  static void SetTypeExtension(Element*, const AtomicString& type);

  void Resolve(Element*, const V0CustomElementDescriptor&);

  bool NameIsDefined(const AtomicString& name) const;
  void SetV1(const CustomElementRegistry*);

  DECLARE_TRACE();

 protected:
  V0CustomElementRegistrationContext();

  // Instance creation
  void DidGiveTypeExtension(Element*, const AtomicString& type);

 private:
  void ResolveOrScheduleResolution(Element*,
                                   const AtomicString& type_extension);

  V0CustomElementRegistry registry_;

  // Element creation
  Member<V0CustomElementUpgradeCandidateMap> candidates_;
};

}  // namespace blink

#endif  // V0CustomElementRegistrationContext_h
