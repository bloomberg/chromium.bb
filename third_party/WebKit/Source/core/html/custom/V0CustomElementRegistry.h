/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef V0CustomElementRegistry_h
#define V0CustomElementRegistry_h

#include "core/html/custom/V0CustomElement.h"
#include "core/html/custom/V0CustomElementDefinition.h"
#include "core/html/custom/V0CustomElementDescriptor.h"
#include "core/html/custom/V0CustomElementDescriptorHash.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class CustomElementRegistry;
class ExceptionState;
class V0CustomElementConstructorBuilder;

class V0CustomElementRegistry final {
  WTF_MAKE_NONCOPYABLE(V0CustomElementRegistry);
  DISALLOW_NEW();

 public:
  DECLARE_TRACE();
  void DocumentWasDetached() { document_was_detached_ = true; }

 protected:
  friend class V0CustomElementRegistrationContext;

  V0CustomElementRegistry() : document_was_detached_(false) {}

  V0CustomElementDefinition* RegisterElement(
      Document*,
      V0CustomElementConstructorBuilder*,
      const AtomicString& name,
      V0CustomElement::NameSet valid_names,
      ExceptionState&);
  V0CustomElementDefinition* Find(const V0CustomElementDescriptor&) const;

  bool NameIsDefined(const AtomicString& name) const;
  void SetV1(const CustomElementRegistry*);

 private:
  bool V1NameIsDefined(const AtomicString& name) const;

  typedef HeapHashMap<V0CustomElementDescriptor,
                      Member<V0CustomElementDefinition>>
      DefinitionMap;
  DefinitionMap definitions_;
  HashSet<AtomicString> registered_type_names_;
  Member<const CustomElementRegistry> v1_;
  bool document_was_detached_;
};

}  // namespace blink

#endif  // V0CustomElementRegistry_h
