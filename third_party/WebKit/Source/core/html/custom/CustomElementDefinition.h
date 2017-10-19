// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/html/custom/CustomElementDescriptor.h"
#include "platform/bindings/ScriptWrappable.h"  // For TraceWrapperBase
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class Document;
class Element;
class ExceptionState;
class HTMLElement;
class QualifiedName;

class CORE_EXPORT CustomElementDefinition
    : public GarbageCollectedFinalized<CustomElementDefinition>,
      public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(CustomElementDefinition);

 public:
  // Each definition has an ID that is unique within the
  // CustomElementRegistry that created it.
  using Id = uint32_t;

  CustomElementDefinition(const CustomElementDescriptor&);
  CustomElementDefinition(const CustomElementDescriptor&,
                          const HashSet<AtomicString>&);
  virtual ~CustomElementDefinition();

  virtual void Trace(blink::Visitor*);
  DECLARE_VIRTUAL_TRACE_WRAPPERS() {}

  const CustomElementDescriptor& Descriptor() { return descriptor_; }

  // TODO(yosin): To support Web Modules, introduce an abstract
  // class |CustomElementConstructor| to allow us to have JavaScript
  // and C++ constructors and ask the binding layer to convert
  // |CustomElementConstructor| to |ScriptValue|. Replace
  // |getConstructorForScript()| by |getConstructor() ->
  // CustomElementConstructor|.
  virtual ScriptValue GetConstructorForScript() = 0;

  using ConstructionStack = HeapVector<Member<Element>, 1>;
  ConstructionStack& GetConstructionStack() { return construction_stack_; }

  HTMLElement* CreateElementForConstructor(Document&);
  virtual HTMLElement* CreateElementSync(Document&, const QualifiedName&) = 0;
  HTMLElement* CreateElementAsync(Document&, const QualifiedName&);

  void Upgrade(Element*);

  virtual bool HasConnectedCallback() const = 0;
  virtual bool HasDisconnectedCallback() const = 0;
  virtual bool HasAdoptedCallback() const = 0;
  bool HasAttributeChangedCallback(const QualifiedName&) const;
  bool HasStyleAttributeChangedCallback() const;

  virtual void RunConnectedCallback(Element*) = 0;
  virtual void RunDisconnectedCallback(Element*) = 0;
  virtual void RunAdoptedCallback(Element*,
                                  Document* old_owner,
                                  Document* new_owner) = 0;
  virtual void RunAttributeChangedCallback(Element*,
                                           const QualifiedName&,
                                           const AtomicString& old_value,
                                           const AtomicString& new_value) = 0;

  void EnqueueUpgradeReaction(Element*);
  void EnqueueConnectedCallback(Element*);
  void EnqueueDisconnectedCallback(Element*);
  void EnqueueAdoptedCallback(Element*,
                              Document* old_owner,
                              Document* new_owner);
  void EnqueueAttributeChangedCallback(Element*,
                                       const QualifiedName&,
                                       const AtomicString& old_value,
                                       const AtomicString& new_value);

  class CORE_EXPORT ConstructionStackScope final {
    STACK_ALLOCATED();
    DISALLOW_COPY_AND_ASSIGN(ConstructionStackScope);

   public:
    ConstructionStackScope(CustomElementDefinition*, Element*);
    ~ConstructionStackScope();

   private:
    ConstructionStack& construction_stack_;
    Member<Element> element_;
    size_t depth_;
  };

 protected:
  virtual bool RunConstructor(Element*) = 0;

  static void CheckConstructorResult(Element*,
                                     Document&,
                                     const QualifiedName&,
                                     ExceptionState&);

 private:
  const CustomElementDescriptor descriptor_;
  ConstructionStack construction_stack_;
  HashSet<AtomicString> observed_attributes_;
  bool has_style_attribute_changed_callback_;

  void EnqueueAttributeChangedCallbackForAllAttributes(Element*);
};

}  // namespace blink

#endif  // CustomElementDefinition_h
