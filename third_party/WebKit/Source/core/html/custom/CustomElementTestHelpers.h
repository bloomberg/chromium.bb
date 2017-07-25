// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElementTestHelpers_h
#define CustomElementTestHelpers_h

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ElementDefinitionOptions.h"
#include "core/dom/QualifiedName.h"
#include "core/html/HTMLDocument.h"
#include "core/html/custom/CEReactionsScope.h"
#include "core/html/custom/CustomElementDefinition.h"
#include "core/html/custom/CustomElementDefinitionBuilder.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"

#include <utility>
#include <vector>

namespace blink {

class CustomElementDescriptor;

class TestCustomElementDefinitionBuilder
    : public CustomElementDefinitionBuilder {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(TestCustomElementDefinitionBuilder);

 public:
  TestCustomElementDefinitionBuilder() {}
  bool CheckConstructorIntrinsics() override { return true; }
  bool CheckConstructorNotRegistered() override { return true; }
  bool CheckPrototype() override { return true; }
  bool RememberOriginalProperties() override { return true; }
  CustomElementDefinition* Build(const CustomElementDescriptor&,
                                 CustomElementDefinition::Id) override;
};

class TestCustomElementDefinition : public CustomElementDefinition {
  WTF_MAKE_NONCOPYABLE(TestCustomElementDefinition);

 public:
  TestCustomElementDefinition(const CustomElementDescriptor& descriptor)
      : CustomElementDefinition(descriptor) {}

  TestCustomElementDefinition(const CustomElementDescriptor& descriptor,
                              HashSet<AtomicString>&& observed_attributes)
      : CustomElementDefinition(descriptor, std::move(observed_attributes)) {}

  ~TestCustomElementDefinition() override = default;

  ScriptValue GetConstructorForScript() override { return ScriptValue(); }

  bool RunConstructor(Element* element) override {
    if (GetConstructionStack().IsEmpty() ||
        GetConstructionStack().back() != element)
      return false;
    GetConstructionStack().back().Clear();
    return true;
  }

  HTMLElement* CreateElementSync(Document& document,
                                 const QualifiedName&) override {
    return CreateElementForConstructor(document);
  }

  bool HasConnectedCallback() const override { return false; }
  bool HasDisconnectedCallback() const override { return false; }
  bool HasAdoptedCallback() const override { return false; }

  void RunConnectedCallback(Element*) override {
    NOTREACHED() << "definition does not have connected callback";
  }

  void RunDisconnectedCallback(Element*) override {
    NOTREACHED() << "definition does not have disconnected callback";
  }

  void RunAdoptedCallback(Element*,
                          Document* old_owner,
                          Document* new_owner) override {
    NOTREACHED() << "definition does not have adopted callback";
  }

  void RunAttributeChangedCallback(Element*,
                                   const QualifiedName&,
                                   const AtomicString& old_value,
                                   const AtomicString& new_value) override {
    NOTREACHED() << "definition does not have attribute changed callback";
  }
};

class CreateElement {
  STACK_ALLOCATED();

 public:
  CreateElement(const AtomicString& local_name)
      : namespace_uri_(HTMLNames::xhtmlNamespaceURI), local_name_(local_name) {}

  CreateElement& InDocument(Document* document) {
    document_ = document;
    return *this;
  }

  CreateElement& InNamespace(const AtomicString& uri) {
    namespace_uri_ = uri;
    return *this;
  }

  CreateElement& WithId(const AtomicString& id) {
    attributes_.push_back(std::make_pair(HTMLNames::idAttr, id));
    return *this;
  }

  CreateElement& WithIsAttribute(const AtomicString& value) {
    attributes_.push_back(std::make_pair(HTMLNames::isAttr, value));
    return *this;
  }

  operator Element*() const {
    Document* document = document_.Get();
    if (!document)
      document = HTMLDocument::CreateForTest();
    NonThrowableExceptionState no_exceptions;
    Element* element =
        document->createElementNS(namespace_uri_, local_name_, no_exceptions);
    for (const auto& attribute : attributes_)
      element->setAttribute(attribute.first, attribute.second);
    return element;
  }

 private:
  Member<Document> document_;
  AtomicString namespace_uri_;
  AtomicString local_name_;
  std::vector<std::pair<QualifiedName, AtomicString>> attributes_;
};

}  // namespace blink

#endif  // CustomElementTestHelpers_h
