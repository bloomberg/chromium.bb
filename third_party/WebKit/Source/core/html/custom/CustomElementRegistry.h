// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementRegistry_h
#define CustomElementRegistry_h

#include "base/gtest_prod_util.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/CoreExport.h"
#include "core/html/custom/CustomElementDefinition.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class CustomElementDefinitionBuilder;
class CustomElementDescriptor;
class CustomElementReactionStack;
class Element;
class ElementDefinitionOptions;
class ExceptionState;
class LocalDOMWindow;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;
class V0CustomElementRegistrationContext;

class CORE_EXPORT CustomElementRegistry final
    : public GarbageCollectedFinalized<CustomElementRegistry>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  WTF_MAKE_NONCOPYABLE(CustomElementRegistry);

 public:
  static CustomElementRegistry* Create(const LocalDOMWindow*);

  virtual ~CustomElementRegistry() = default;

  CustomElementDefinition* define(ScriptState*,
                                  const AtomicString& name,
                                  const ScriptValue& constructor,
                                  const ElementDefinitionOptions&,
                                  ExceptionState&);

  CustomElementDefinition* define(const AtomicString& name,
                                  CustomElementDefinitionBuilder&,
                                  const ElementDefinitionOptions&,
                                  ExceptionState&);

  ScriptValue get(const AtomicString& name);
  bool NameIsDefined(const AtomicString& name) const;
  CustomElementDefinition* DefinitionForName(const AtomicString& name) const;
  CustomElementDefinition* DefinitionForId(CustomElementDefinition::Id) const;

  // TODO(dominicc): Switch most callers of definitionForName to
  // definitionFor when implementing type extensions.
  CustomElementDefinition* DefinitionFor(const CustomElementDescriptor&) const;

  // TODO(dominicc): Consider broadening this API when type extensions are
  // implemented.
  void AddCandidate(Element*);
  ScriptPromise whenDefined(ScriptState*,
                            const AtomicString& name,
                            ExceptionState&);

  void Entangle(V0CustomElementRegistrationContext*);

  void Trace(blink::Visitor*);
  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  friend class CustomElementRegistryTest;

  CustomElementRegistry(const LocalDOMWindow*);

  bool V0NameIsDefined(const AtomicString& name);

  void CollectCandidates(const CustomElementDescriptor&,
                         HeapVector<Member<Element>>*);

  class ElementDefinitionIsRunning;
  bool element_definition_is_running_;

  using DefinitionList =
      HeapVector<TraceWrapperMember<CustomElementDefinition>>;
  DefinitionList definitions_;

  using NameIdMap = HashMap<AtomicString, size_t>;
  NameIdMap name_id_map_;

  Member<const LocalDOMWindow> owner_;

  using V0RegistrySet =
      HeapHashSet<WeakMember<V0CustomElementRegistrationContext>>;
  Member<V0RegistrySet> v0_;

  using UpgradeCandidateSet = HeapHashSet<WeakMember<Element>>;
  using UpgradeCandidateMap =
      HeapHashMap<AtomicString, Member<UpgradeCandidateSet>>;
  Member<UpgradeCandidateMap> upgrade_candidates_;

  using WhenDefinedPromiseMap =
      HeapHashMap<AtomicString, Member<ScriptPromiseResolver>>;
  WhenDefinedPromiseMap when_defined_promise_map_;

  TraceWrapperMember<CustomElementReactionStack> reaction_stack_;
};

}  // namespace blink

#endif  // CustomElementRegistry_h
