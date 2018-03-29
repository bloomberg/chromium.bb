// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementDefinition_h
#define CustomElementDefinition_h

#include "base/macros.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/CreateElementFlags.h"
#include "core/html/custom/CustomElementDescriptor.h"
#include "platform/bindings/ScriptWrappable.h"  // For TraceWrapperBase
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class CSSStyleSheet;
class Document;
class Element;
class ExceptionState;
class HTMLElement;
class QualifiedName;

class CORE_EXPORT CustomElementDefinition
    : public GarbageCollectedFinalized<CustomElementDefinition>,
      public TraceWrapperBase {
 public:
  // Each definition has an ID that is unique within the
  // CustomElementRegistry that created it.
  using Id = uint32_t;

  virtual ~CustomElementDefinition();

  virtual void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor* visitor) const override {}
  const char* NameInHeapSnapshot() const override {
    return "CustomElementDefinition";
  }

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
  virtual HTMLElement* CreateAutonomousCustomElementSync(
      Document&,
      const QualifiedName&) = 0;
  HTMLElement* CreateElement(Document&,
                             const QualifiedName&,
                             const CreateElementFlags);

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

  CSSStyleSheet* DefaultStyleSheet() const { return default_style_sheet_; }

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
  CustomElementDefinition(const CustomElementDescriptor&);

  CustomElementDefinition(const CustomElementDescriptor&, CSSStyleSheet*);

  CustomElementDefinition(const CustomElementDescriptor&,
                          CSSStyleSheet*,
                          const HashSet<AtomicString>& observed_attributes);

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

  const Member<CSSStyleSheet> default_style_sheet_;

  void EnqueueAttributeChangedCallbackForAllAttributes(Element*);

  DISALLOW_COPY_AND_ASSIGN(CustomElementDefinition);
};

}  // namespace blink

#endif  // CustomElementDefinition_h
