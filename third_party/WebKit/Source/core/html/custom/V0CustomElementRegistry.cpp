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

#include "core/html/custom/V0CustomElementRegistry.h"

#include "bindings/core/v8/V0CustomElementConstructorBuilder.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/dom/Document.h"
#include "core/frame/UseCounter.h"
#include "core/html/custom/CustomElementRegistry.h"
#include "core/html/custom/V0CustomElementException.h"
#include "core/html/custom/V0CustomElementRegistrationContext.h"

namespace blink {

V0CustomElementDefinition* V0CustomElementRegistry::RegisterElement(
    Document* document,
    V0CustomElementConstructorBuilder* constructor_builder,
    const AtomicString& user_supplied_name,
    V0CustomElement::NameSet valid_names,
    ExceptionState& exception_state) {
  AtomicString type = user_supplied_name.LowerASCII();

  if (!constructor_builder->IsFeatureAllowed()) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kCannotRegisterFromExtension, type,
        exception_state);
    return nullptr;
  }

  if (!V0CustomElement::IsValidName(type, valid_names)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kInvalidName, type, exception_state);
    return nullptr;
  }

  if (registered_type_names_.Contains(type) || V1NameIsDefined(type)) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kTypeAlreadyRegistered, type,
        exception_state);
    return nullptr;
  }

  QualifiedName tag_name = QualifiedName::Null();
  if (!constructor_builder->ValidateOptions(type, tag_name, exception_state))
    return nullptr;

  DCHECK(tag_name.NamespaceURI() == HTMLNames::xhtmlNamespaceURI ||
         tag_name.NamespaceURI() == SVGNames::svgNamespaceURI);

  DCHECK(!document_was_detached_);

  V0CustomElementLifecycleCallbacks* lifecycle_callbacks =
      constructor_builder->CreateCallbacks();

  // Consulting the constructor builder could execute script and
  // kill the document.
  if (document_was_detached_) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kContextDestroyedCreatingCallbacks, type,
        exception_state);
    return nullptr;
  }

  const V0CustomElementDescriptor descriptor(type, tag_name.NamespaceURI(),
                                             tag_name.LocalName());
  V0CustomElementDefinition* definition =
      V0CustomElementDefinition::Create(descriptor, lifecycle_callbacks);

  if (!constructor_builder->CreateConstructor(document, definition,
                                              exception_state))
    return nullptr;

  definitions_.insert(descriptor, definition);
  registered_type_names_.insert(descriptor.GetType());

  if (!constructor_builder->DidRegisterDefinition()) {
    V0CustomElementException::ThrowException(
        V0CustomElementException::kContextDestroyedRegisteringDefinition, type,
        exception_state);
    return nullptr;
  }

  if (valid_names & V0CustomElement::kEmbedderNames) {
    UseCounter::Count(document,
                      WebFeature::kV0CustomElementsRegisterEmbedderElement);
  } else if (tag_name.NamespaceURI() == SVGNames::svgNamespaceURI) {
    UseCounter::Count(document,
                      WebFeature::kV0CustomElementsRegisterSVGElement);
  } else {
    UseCounter::Count(
        document, descriptor.IsTypeExtension()
                      ? WebFeature::kV0CustomElementsRegisterHTMLTypeExtension
                      : WebFeature::kV0CustomElementsRegisterHTMLCustomTag);
  }

  return definition;
}

V0CustomElementDefinition* V0CustomElementRegistry::Find(
    const V0CustomElementDescriptor& descriptor) const {
  return definitions_.at(descriptor);
}

bool V0CustomElementRegistry::NameIsDefined(const AtomicString& name) const {
  return registered_type_names_.Contains(name);
}

void V0CustomElementRegistry::SetV1(const CustomElementRegistry* v1) {
  DCHECK(!v1_.Get());
  v1_ = v1;
}

bool V0CustomElementRegistry::V1NameIsDefined(const AtomicString& name) const {
  return v1_.Get() && v1_->NameIsDefined(name);
}

DEFINE_TRACE(V0CustomElementRegistry) {
  visitor->Trace(definitions_);
  visitor->Trace(v1_);
}

}  // namespace blink
