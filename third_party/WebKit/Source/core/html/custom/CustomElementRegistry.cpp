// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementRegistry.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinitionBuilder.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/HTMLElementTypeHelpers.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementDefinitionOptions.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/custom/CEReactionsScope.h"
#include "core/html/custom/CustomElement.h"
#include "core/html/custom/CustomElementDefinition.h"
#include "core/html/custom/CustomElementDefinitionBuilder.h"
#include "core/html/custom/CustomElementDescriptor.h"
#include "core/html/custom/CustomElementReactionStack.h"
#include "core/html/custom/CustomElementUpgradeReaction.h"
#include "core/html/custom/CustomElementUpgradeSorter.h"
#include "core/html/custom/V0CustomElementRegistrationContext.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/Allocator.h"

#include <limits>

namespace blink {

// Returns true if |name| is invalid.
static bool ThrowIfInvalidName(const AtomicString& name,
                               ExceptionState& exception_state) {
  if (CustomElement::IsValidName(name))
    return false;
  exception_state.ThrowDOMException(
      kSyntaxError, "\"" + name + "\" is not a valid custom element name");
  return true;
}

// Returns true if |name| is valid.
static bool ThrowIfValidName(const AtomicString& name,
                             ExceptionState& exception_state) {
  if (!CustomElement::IsValidName(name))
    return false;
  exception_state.ThrowDOMException(
      kNotSupportedError, "\"" + name + "\" is a valid custom element name");
  return true;
}

class CustomElementRegistry::ElementDefinitionIsRunning final {
  STACK_ALLOCATED();
  DISALLOW_IMPLICIT_CONSTRUCTORS(ElementDefinitionIsRunning);

 public:
  ElementDefinitionIsRunning(bool& flag) : flag_(flag) {
    DCHECK(!flag_);
    flag_ = true;
  }

  ~ElementDefinitionIsRunning() {
    DCHECK(flag_);
    flag_ = false;
  }

 private:
  bool& flag_;
};

CustomElementRegistry* CustomElementRegistry::Create(
    const LocalDOMWindow* owner) {
  CustomElementRegistry* registry = new CustomElementRegistry(owner);
  Document* document = owner->document();
  if (V0CustomElementRegistrationContext* v0 =
          document ? document->RegistrationContext() : nullptr)
    registry->Entangle(v0);
  return registry;
}

CustomElementRegistry::CustomElementRegistry(const LocalDOMWindow* owner)
    : element_definition_is_running_(false),
      owner_(owner),
      v0_(new V0RegistrySet()),
      upgrade_candidates_(new UpgradeCandidateMap()) {}

DEFINE_TRACE(CustomElementRegistry) {
  visitor->Trace(definitions_);
  visitor->Trace(owner_);
  visitor->Trace(v0_);
  visitor->Trace(upgrade_candidates_);
  visitor->Trace(when_defined_promise_map_);
}

DEFINE_TRACE_WRAPPERS(CustomElementRegistry) {
  visitor->TraceWrappers(&CustomElementReactionStack::Current());
  for (auto definition : definitions_)
    visitor->TraceWrappers(definition);
}

CustomElementDefinition* CustomElementRegistry::define(
    ScriptState* script_state,
    const AtomicString& name,
    const ScriptValue& constructor,
    const ElementDefinitionOptions& options,
    ExceptionState& exception_state) {
  ScriptCustomElementDefinitionBuilder builder(script_state, this, constructor,
                                               exception_state);
  return define(name, builder, options, exception_state);
}

// http://w3c.github.io/webcomponents/spec/custom/#dfn-element-definition
CustomElementDefinition* CustomElementRegistry::define(
    const AtomicString& name,
    CustomElementDefinitionBuilder& builder,
    const ElementDefinitionOptions& options,
    ExceptionState& exception_state) {
  TRACE_EVENT1("blink", "CustomElementRegistry::define", "name", name.Utf8());
  if (!builder.CheckConstructorIntrinsics())
    return nullptr;

  if (ThrowIfInvalidName(name, exception_state))
    return nullptr;

  if (NameIsDefined(name) || V0NameIsDefined(name)) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        "this name has already been used with this registry");
    return nullptr;
  }

  if (!builder.CheckConstructorNotRegistered())
    return nullptr;

  AtomicString local_name = name;

  // Step 7. customized built-in elements definition
  // element interface extends option checks
  if (RuntimeEnabledFeatures::CustomElementsBuiltinEnabled() &&
      options.hasExtends()) {
    // 7.1. If element interface is valid custom element name, throw exception
    const AtomicString& extends = AtomicString(options.extends());
    if (ThrowIfValidName(AtomicString(options.extends()), exception_state))
      return nullptr;
    // 7.2. If element interface is undefined element, throw exception
    if (htmlElementTypeForTag(extends) ==
        HTMLElementType::kHTMLUnknownElement) {
      exception_state.ThrowDOMException(
          kNotSupportedError, "\"" + extends + "\" is an HTMLUnknownElement");
      return nullptr;
    }
    // 7.3. Set localName to extends
    local_name = extends;
  }

  // TODO(dominicc): Add a test where the prototype getter destroys
  // the context.

  // 8. If this CustomElementRegistry's element definition is
  // running flag is set, then throw a "NotSupportedError"
  // DOMException and abort these steps.
  if (element_definition_is_running_) {
    exception_state.ThrowDOMException(
        kNotSupportedError, "an element definition is already being processed");
    return nullptr;
  }

  {
    // 9. Set this CustomElementRegistry's element definition is
    // running flag.
    ElementDefinitionIsRunning defining(element_definition_is_running_);

    // 10.1-2
    if (!builder.CheckPrototype())
      return nullptr;

    // 10.3-6
    if (!builder.RememberOriginalProperties())
      return nullptr;

    // "Then, perform the following substep, regardless of whether
    // the above steps threw an exception or not: Unset this
    // CustomElementRegistry's element definition is running
    // flag."
    // (ElementDefinitionIsRunning destructor does this.)
  }

  CustomElementDescriptor descriptor(name, local_name);
  if (UNLIKELY(definitions_.size() >=
               std::numeric_limits<CustomElementDefinition::Id>::max()))
    return nullptr;
  CustomElementDefinition::Id id = definitions_.size() + 1;
  CustomElementDefinition* definition = builder.Build(descriptor, id);
  CHECK(!exception_state.HadException());
  CHECK(definition->Descriptor() == descriptor);
  definitions_.emplace_back(
      TraceWrapperMember<CustomElementDefinition>(this, definition));
  NameIdMap::AddResult result = name_id_map_.insert(descriptor.GetName(), id);
  CHECK(result.is_new_entry);

  HeapVector<Member<Element>> candidates;
  CollectCandidates(descriptor, &candidates);
  for (Element* candidate : candidates)
    definition->EnqueueUpgradeReaction(candidate);

  // 16: when-defined promise processing
  const auto& entry = when_defined_promise_map_.find(name);
  if (entry != when_defined_promise_map_.end()) {
    entry->value->Resolve();
    when_defined_promise_map_.erase(entry);
  }
  return definition;
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-get
ScriptValue CustomElementRegistry::get(const AtomicString& name) {
  CustomElementDefinition* definition = DefinitionForName(name);
  if (!definition) {
    // Binding layer converts |ScriptValue()| to script specific value,
    // e.g. |undefined| for v8.
    return ScriptValue();
  }
  return definition->GetConstructorForScript();
}

// https://html.spec.whatwg.org/multipage/scripting.html#look-up-a-custom-element-definition
// At this point, what the spec calls 'is' is 'name' from desc
CustomElementDefinition* CustomElementRegistry::DefinitionFor(
    const CustomElementDescriptor& desc) const {
  // desc.name() is 'is' attribute
  // 4. If definition in registry with name equal to local name...
  CustomElementDefinition* definition = DefinitionForName(desc.LocalName());
  // 5. If definition in registry with name equal to name...
  if (!definition)
    definition = DefinitionForName(desc.GetName());
  // 4&5. ...and local name equal to localName, return that definition
  if (definition and definition->Descriptor().LocalName() == desc.LocalName()) {
    return definition;
  }
  // 6. Return null
  return nullptr;
}

bool CustomElementRegistry::NameIsDefined(const AtomicString& name) const {
  return name_id_map_.Contains(name);
}

void CustomElementRegistry::Entangle(V0CustomElementRegistrationContext* v0) {
  v0_->insert(v0);
  v0->SetV1(this);
}

bool CustomElementRegistry::V0NameIsDefined(const AtomicString& name) {
  for (const auto& v0 : *v0_) {
    if (v0->NameIsDefined(name))
      return true;
  }
  return false;
}

CustomElementDefinition* CustomElementRegistry::DefinitionForName(
    const AtomicString& name) const {
  return DefinitionForId(name_id_map_.at(name));
}

CustomElementDefinition* CustomElementRegistry::DefinitionForId(
    CustomElementDefinition::Id id) const {
  return id ? definitions_[id - 1].Get() : nullptr;
}

void CustomElementRegistry::AddCandidate(Element* candidate) {
  const AtomicString& name = candidate->localName();
  if (NameIsDefined(name) || V0NameIsDefined(name))
    return;
  UpgradeCandidateMap::iterator it = upgrade_candidates_->find(name);
  UpgradeCandidateSet* set;
  if (it != upgrade_candidates_->end()) {
    set = it->value;
  } else {
    set = upgrade_candidates_->insert(name, new UpgradeCandidateSet())
              .stored_value->value;
  }
  set->insert(candidate);
}

// https://html.spec.whatwg.org/multipage/scripting.html#dom-customelementsregistry-whendefined
ScriptPromise CustomElementRegistry::whenDefined(
    ScriptState* script_state,
    const AtomicString& name,
    ExceptionState& exception_state) {
  if (ThrowIfInvalidName(name, exception_state))
    return ScriptPromise();
  CustomElementDefinition* definition = DefinitionForName(name);
  if (definition)
    return ScriptPromise::CastUndefined(script_state);
  ScriptPromiseResolver* resolver = when_defined_promise_map_.at(name);
  if (resolver)
    return resolver->Promise();
  ScriptPromiseResolver* new_resolver =
      ScriptPromiseResolver::Create(script_state);
  when_defined_promise_map_.insert(name, new_resolver);
  return new_resolver->Promise();
}

void CustomElementRegistry::CollectCandidates(
    const CustomElementDescriptor& desc,
    HeapVector<Member<Element>>* elements) {
  UpgradeCandidateMap::iterator it = upgrade_candidates_->find(desc.GetName());
  if (it == upgrade_candidates_->end())
    return;
  CustomElementUpgradeSorter sorter;
  for (Element* element : *it.Get()->value) {
    if (!element || !desc.Matches(*element))
      continue;
    sorter.Add(element);
  }

  upgrade_candidates_->erase(it);

  Document* document = owner_->document();
  if (!document)
    return;

  sorter.Sorted(elements, document);
}

}  // namespace blink
