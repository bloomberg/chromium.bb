/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "third_party/blink/renderer/core/editing/serializers/markup_accumulator.h"

#include "third_party/blink/renderer/core/dom/attr.h"
#include "third_party/blink/renderer/core/dom/cdata_section.h"
#include "third_party/blink/renderer/core/dom/comment.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_type.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/core/xml_names.h"
#include "third_party/blink/renderer/core/xmlns_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

class MarkupAccumulator::NamespaceContext final {
  USING_FAST_MALLOC(MarkupAccumulator::NamespaceContext);

 public:
  // https://w3c.github.io/DOM-Parsing/#dfn-add
  void Add(const AtomicString& prefix, const AtomicString& namespace_uri) {
    const AtomicString& non_null_prefix = prefix ? prefix : g_empty_atom;
    prefix_ns_map_.Set(non_null_prefix, namespace_uri);
    auto result = ns_prefixes_map_.insert(
        namespace_uri ? namespace_uri : g_empty_atom, Vector<AtomicString>());
    result.stored_value->value.push_back(non_null_prefix);
  }

  AtomicString LookupNamespaceURI(const AtomicString& prefix) const {
    return prefix_ns_map_.at(prefix ? prefix : g_empty_atom);
  }

  const Vector<AtomicString> PrefixList(const AtomicString& ns) const {
    return ns_prefixes_map_.at(ns ? ns : g_empty_atom);
  }

 private:
  using PrefixToNamespaceMap = HashMap<AtomicString, AtomicString>;
  PrefixToNamespaceMap prefix_ns_map_;

  // Map a namespace URI to a list of prefixes.
  // https://w3c.github.io/DOM-Parsing/#the-namespace-prefix-map
  using NamespaceToPrefixesMap = HashMap<AtomicString, Vector<AtomicString>>;
  NamespaceToPrefixesMap ns_prefixes_map_;

  // TODO(tkent): Add 'context namespace' field. It is required to fix
  // crbug.com/929035.
  // https://w3c.github.io/DOM-Parsing/#dfn-context-namespace
};

MarkupAccumulator::MarkupAccumulator(EAbsoluteURLs resolve_urls_method,
                                     SerializationType serialization_type)
    : formatter_(resolve_urls_method, serialization_type) {}

MarkupAccumulator::~MarkupAccumulator() = default;

void MarkupAccumulator::AppendString(const String& string) {
  markup_.Append(string);
}

void MarkupAccumulator::AppendEndTag(const Element& element) {
  formatter_.AppendEndMarkup(markup_, element);
}

void MarkupAccumulator::AppendStartMarkup(const Node& node) {
  switch (node.getNodeType()) {
    case Node::kTextNode:
      formatter_.AppendText(markup_, ToText(node));
      break;
    case Node::kElementNode:
      NOTREACHED();
      break;
    case Node::kAttributeNode:
      // Only XMLSerializer can pass an Attr.  So, |documentIsHTML| flag is
      // false.
      formatter_.AppendAttributeValue(markup_, ToAttr(node).value(), false);
      break;
    default:
      formatter_.AppendStartMarkup(markup_, node);
      break;
  }
}

void MarkupAccumulator::AppendCustomAttributes(const Element&) {}

bool MarkupAccumulator::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) const {
  return false;
}

bool MarkupAccumulator::ShouldIgnoreElement(const Element& element) const {
  return false;
}

void MarkupAccumulator::AppendElement(const Element& element) {
  // https://html.spec.whatwg.org/multipage/parsing.html#html-fragment-serialisation-algorithm
  RecordNamespaceInformation(element);
  AppendStartTagOpen(element);

  AttributeCollection attributes = element.Attributes();
  if (SerializeAsHTMLDocument(element)) {
    // 3.2. Element: If current node's is value is not null, and the
    // element does not have an is attribute in its attribute list, ...
    const AtomicString& is_value = element.IsValue();
    if (!is_value.IsNull() && !attributes.Find(html_names::kIsAttr)) {
      AppendAttribute(element, Attribute(html_names::kIsAttr, is_value));
    }
  }
  for (const auto& attribute : attributes) {
    if (!ShouldIgnoreAttribute(element, attribute))
      AppendAttribute(element, attribute);
  }

  // Give an opportunity to subclasses to add their own attributes.
  AppendCustomAttributes(element);

  AppendStartTagClose(element);
}

void MarkupAccumulator::AppendStartTagOpen(const Element& element) {
  formatter_.AppendStartTagOpen(markup_, element);
  if (!SerializeAsHTMLDocument(element) && ShouldAddNamespaceElement(element)) {
    AppendNamespace(element.prefix(), element.namespaceURI());
  }
}

void MarkupAccumulator::AppendStartTagClose(const Element& element) {
  formatter_.AppendStartTagClose(markup_, element);
}

void MarkupAccumulator::AppendAttribute(const Element& element,
                                        const Attribute& attribute) {
  String value = formatter_.ResolveURLIfNeeded(element, attribute);
  if (SerializeAsHTMLDocument(element)) {
    MarkupFormatter::AppendAttributeAsHTML(markup_, attribute, value);
  } else {
    AppendAttributeAsXMLWithNamespace(element, attribute, value);
  }
}

void MarkupAccumulator::AppendAttributeAsXMLWithNamespace(
    const Element& element,
    const Attribute& attribute,
    const String& value) {
  // https://w3c.github.io/DOM-Parsing/#serializing-an-element-s-attributes

  // 3.3. Let attribute namespace be the value of attr's namespaceURI value.
  const AtomicString& attribute_namespace = attribute.NamespaceURI();

  // 3.4. Let candidate prefix be null.
  AtomicString candidate_prefix;

  if (attribute_namespace.IsNull()) {
    MarkupFormatter::AppendAttribute(markup_, candidate_prefix,
                                     attribute.LocalName(), value, false);
    return;
  }
  // 3.5. If attribute namespace is not null, then run these sub-steps:

  // 3.5.1. Let candidate prefix be the result of retrieving a preferred
  // prefix string from map given namespace attribute namespace with preferred
  // prefix being attr's prefix value.
  candidate_prefix =
      RetrievePreferredPrefixString(attribute_namespace, attribute.Prefix());

  // 3.5.2. If the value of attribute namespace is the XMLNS namespace, then
  // run these steps:
  if (attribute_namespace == xmlns_names::kNamespaceURI) {
    if (!attribute.Prefix() && attribute.LocalName() != g_xmlns_atom)
      candidate_prefix = g_xmlns_atom;
  } else {
    // TODO(tkent): Remove this block. The standard and Firefox don't
    // have this behavior.
    if (attribute_namespace == xlink_names::kNamespaceURI) {
      if (!candidate_prefix)
        candidate_prefix = g_xlink_atom;
    }

    // 3.5.3. Otherwise, the attribute namespace in not the XMLNS namespace.
    // Run these steps:
    if (ShouldAddNamespaceAttribute(attribute, candidate_prefix)) {
      if (!candidate_prefix || LookupNamespaceURI(candidate_prefix)) {
        // 3.5.3.1. Let candidate prefix be the result of generating a prefix
        // providing map, attribute namespace, and prefix index as input.
        candidate_prefix = GeneratePrefix(attribute_namespace);
        // 3.5.3.2. Append the following to result, in the order listed:
        MarkupFormatter::AppendAttribute(markup_, g_xmlns_atom,
                                         candidate_prefix, attribute_namespace,
                                         false);
      } else {
        DCHECK(candidate_prefix);
        AppendNamespace(candidate_prefix, attribute_namespace);
      }
    }
  }
  MarkupFormatter::AppendAttribute(markup_, candidate_prefix,
                                   attribute.LocalName(), value, false);
}

bool MarkupAccumulator::ShouldAddNamespaceAttribute(
    const Attribute& attribute,
    const AtomicString& candidate_prefix) {
  // xmlns and xmlns:prefix attributes should be handled by another branch in
  // AppendAttributeAsXMLWithNamespace().
  DCHECK_NE(attribute.NamespaceURI(), xmlns_names::kNamespaceURI);
  // Null namespace is checked earlier in AppendAttributeAsXMLWithNamespace().
  DCHECK(attribute.NamespaceURI());

  // Attributes without a prefix will need one generated for them, and an xmlns
  // attribute for that prefix.
  if (!candidate_prefix)
    return true;

  return !EqualIgnoringNullity(LookupNamespaceURI(candidate_prefix),
                               attribute.NamespaceURI());
}

void MarkupAccumulator::AppendNamespace(const AtomicString& prefix,
                                        const AtomicString& namespace_uri) {
  AtomicString found_uri = LookupNamespaceURI(prefix);
  if (!EqualIgnoringNullity(found_uri, namespace_uri)) {
    AddPrefix(prefix, namespace_uri);
    if (prefix.IsEmpty()) {
      MarkupFormatter::AppendAttribute(markup_, g_null_atom, g_xmlns_atom,
                                       namespace_uri, false);
    } else {
      MarkupFormatter::AppendAttribute(markup_, g_xmlns_atom, prefix,
                                       namespace_uri, false);
    }
  }
}

EntityMask MarkupAccumulator::EntityMaskForText(const Text& text) const {
  return formatter_.EntityMaskForText(text);
}

void MarkupAccumulator::PushNamespaces(const Element& element) {
  if (SerializeAsHTMLDocument(element))
    return;
  DCHECK_GT(namespace_stack_.size(), 0u);
  // TODO(tkent): Avoid to copy the whole map.
  // We can't do |namespace_stack_.emplace_back(namespace_stack_.back())|
  // because back() returns a reference in the vector backing, and
  // emplace_back() can reallocate it.
  namespace_stack_.push_back(NamespaceContext(namespace_stack_.back()));
}

void MarkupAccumulator::PopNamespaces(const Element& element) {
  if (SerializeAsHTMLDocument(element))
    return;
  namespace_stack_.pop_back();
}

// https://w3c.github.io/DOM-Parsing/#dfn-recording-the-namespace-information
void MarkupAccumulator::RecordNamespaceInformation(const Element& element) {
  if (SerializeAsHTMLDocument(element))
    return;
  for (const auto& attr : element.Attributes()) {
    if (attr.NamespaceURI() == xmlns_names::kNamespaceURI)
      AddPrefix(attr.Prefix() ? attr.LocalName() : g_empty_atom, attr.Value());
  }
}

// https://w3c.github.io/DOM-Parsing/#dfn-retrieving-a-preferred-prefix-string
AtomicString MarkupAccumulator::RetrievePreferredPrefixString(
    const AtomicString& ns,
    const AtomicString& preferred_prefix) {
  // TODO(tkent): We'll apply this function to elements too.
  const bool kForAttribute = true;
  AtomicString ns_for_preferred = LookupNamespaceURI(preferred_prefix);
  // Preserve the prefix if the prefix is used in the scope and the namespace
  // for it is matches to the node's one.
  // This is equivalent to the following step in the specification:
  // 2.1. If prefix matches preferred prefix, then stop running these steps and
  // return prefix.
  if ((!kForAttribute || !preferred_prefix.IsEmpty()) &&
      !ns_for_preferred.IsNull() && EqualIgnoringNullity(ns_for_preferred, ns))
    return preferred_prefix;

  const Vector<AtomicString>& candidate_list =
      namespace_stack_.back().PrefixList(ns);
  // Get the last effective prefix.
  //
  // <el1 xmlns:p="U1" xmlns:q="U1">
  //   <el2 xmlns:q="U2">
  //    el2.setAttributeNS(U1, 'n', 'v');
  // We should get 'p'.
  //
  // <el1 xmlns="U1">
  //  el1.setAttributeNS(U1, 'n', 'v');
  // We should not get '' for attributes.
  for (auto it = candidate_list.rbegin(); it != candidate_list.rend(); ++it) {
    AtomicString candidate_prefix = *it;
    if (kForAttribute && candidate_prefix.IsEmpty())
      continue;
    AtomicString ns_for_candaite = LookupNamespaceURI(candidate_prefix);
    if (EqualIgnoringNullity(ns_for_candaite, ns))
      return candidate_prefix;
  }

  // No prefixes for |ns|.
  // Preserve the prefix if the prefix is not used in the current scope.
  if (!preferred_prefix.IsEmpty() && ns_for_preferred.IsNull())
    return preferred_prefix;
  // If a prefix is not specified, or the prefix is mapped to a
  // different namespace, we should generate new prefix.
  return g_null_atom;
}

void MarkupAccumulator::AddPrefix(const AtomicString& prefix,
                                  const AtomicString& namespace_uri) {
  namespace_stack_.back().Add(prefix, namespace_uri);
}

AtomicString MarkupAccumulator::LookupNamespaceURI(const AtomicString& prefix) {
  return namespace_stack_.back().LookupNamespaceURI(prefix);
}

// https://w3c.github.io/DOM-Parsing/#dfn-generating-a-prefix
AtomicString MarkupAccumulator::GeneratePrefix(
    const AtomicString& new_namespace) {
  AtomicString generated_prefix;
  do {
    // 1. Let generated prefix be the concatenation of the string "ns" and the
    // current numerical value of prefix index.
    generated_prefix = "ns" + String::Number(prefix_index_);
    // 2. Let the value of prefix index be incremented by one.
    ++prefix_index_;
  } while (LookupNamespaceURI(generated_prefix));
  // 3. Add to map the generated prefix given the new namespace namespace.
  AddPrefix(generated_prefix, new_namespace);
  // 4. Return the value of generated prefix.
  return generated_prefix;
}

bool MarkupAccumulator::SerializeAsHTMLDocument(const Node& node) const {
  return formatter_.SerializeAsHTMLDocument(node);
}

bool MarkupAccumulator::ShouldAddNamespaceElement(const Element& element) {
  // Don't add namespace attribute if it is already defined for this elem.
  const AtomicString& prefix = element.prefix();
  if (prefix.IsEmpty()) {
    if (element.hasAttribute(g_xmlns_atom)) {
      AddPrefix(g_empty_atom, element.namespaceURI());
      return false;
    }
    return true;
  }

  return !element.hasAttribute(WTF::g_xmlns_with_colon + prefix);
}

std::pair<Node*, Element*> MarkupAccumulator::GetAuxiliaryDOMTree(
    const Element& element) const {
  return std::pair<Node*, Element*>();
}

template <typename Strategy>
void MarkupAccumulator::SerializeNodesWithNamespaces(
    const Node& target_node,
    EChildrenOnly children_only) {
  if (!target_node.IsElementNode()) {
    if (!children_only)
      AppendStartMarkup(target_node);
    for (const Node& child : Strategy::ChildrenOf(target_node))
      SerializeNodesWithNamespaces<Strategy>(child, kIncludeNode);
    return;
  }

  const Element& target_element = ToElement(target_node);
  if (ShouldIgnoreElement(target_element))
    return;

  PushNamespaces(target_element);

  if (!children_only)
    AppendElement(target_element);

  bool has_end_tag = !(SerializeAsHTMLDocument(target_element) &&
                       ElementCannotHaveEndTag(target_element));
  if (has_end_tag) {
    const Node* parent = &target_element;
    if (auto* template_element = ToHTMLTemplateElementOrNull(target_element))
      parent = template_element->content();
    for (const Node& child : Strategy::ChildrenOf(*parent))
      SerializeNodesWithNamespaces<Strategy>(child, kIncludeNode);

    // Traverses other DOM tree, i.e., shadow tree.
    std::pair<Node*, Element*> auxiliary_pair =
        GetAuxiliaryDOMTree(target_element);
    if (Node* auxiliary_tree = auxiliary_pair.first) {
      Element* enclosing_element = auxiliary_pair.second;
      if (enclosing_element)
        AppendElement(*enclosing_element);
      for (const Node& child : Strategy::ChildrenOf(*auxiliary_tree))
        SerializeNodesWithNamespaces<Strategy>(child, kIncludeNode);
      if (enclosing_element)
        AppendEndTag(*enclosing_element);
    }

    if (!children_only)
      AppendEndTag(target_element);
  }

  PopNamespaces(target_element);
}

template <typename Strategy>
String MarkupAccumulator::SerializeNodes(const Node& target_node,
                                         EChildrenOnly children_only) {
  if (!SerializeAsHTMLDocument(target_node)) {
    // https://w3c.github.io/DOM-Parsing/#dfn-xml-serialization
    DCHECK_EQ(namespace_stack_.size(), 0u);
    // 2. Let prefix map be a new namespace prefix map.
    namespace_stack_.emplace_back();
    // 3. Add the XML namespace with prefix value "xml" to prefix map.
    AddPrefix(g_xml_atom, xml_names::kNamespaceURI);
    // 4. Let prefix index be a generated namespace prefix index with value 1.
    prefix_index_ = 1;
  }

  SerializeNodesWithNamespaces<Strategy>(target_node, children_only);
  return ToString();
}

template String MarkupAccumulator::SerializeNodes<EditingStrategy>(
    const Node&,
    EChildrenOnly);

}  // namespace blink
