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

#include "core/editing/serializers/MarkupAccumulator.h"

#include "core/XLinkNames.h"
#include "core/XMLNSNames.h"
#include "core/XMLNames.h"
#include "core/dom/Attr.h"
#include "core/dom/CDATASection.h"
#include "core/dom/Comment.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/DocumentType.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/text/CharacterNames.h"

namespace blink {

MarkupAccumulator::MarkupAccumulator(EAbsoluteURLs resolve_urls_method,
                                     SerializationType serialization_type)
    : formatter_(resolve_urls_method, serialization_type) {}

MarkupAccumulator::~MarkupAccumulator() {}

void MarkupAccumulator::AppendString(const String& string) {
  markup_.Append(string);
}

void MarkupAccumulator::AppendStartTag(Node& node, Namespaces* namespaces) {
  AppendStartMarkup(markup_, node, namespaces);
}

void MarkupAccumulator::AppendEndTag(const Element& element) {
  AppendEndMarkup(markup_, element);
}

void MarkupAccumulator::AppendStartMarkup(StringBuilder& result,
                                          Node& node,
                                          Namespaces* namespaces) {
  switch (node.getNodeType()) {
    case Node::kTextNode:
      AppendText(result, ToText(node));
      break;
    case Node::kElementNode:
      AppendElement(result, ToElement(node), namespaces);
      break;
    case Node::kAttributeNode:
      // Only XMLSerializer can pass an Attr.  So, |documentIsHTML| flag is
      // false.
      formatter_.AppendAttributeValue(result, ToAttr(node).value(), false);
      break;
    default:
      formatter_.AppendStartMarkup(result, node, namespaces);
      break;
  }
}

void MarkupAccumulator::AppendEndMarkup(StringBuilder& result,
                                        const Element& element) {
  formatter_.AppendEndMarkup(result, element);
}

void MarkupAccumulator::AppendCustomAttributes(StringBuilder&,
                                               const Element&,
                                               Namespaces*) {}

void MarkupAccumulator::AppendText(StringBuilder& result, Text& text) {
  formatter_.AppendText(result, text);
}

bool MarkupAccumulator::ShouldIgnoreAttribute(
    const Element& element,
    const Attribute& attribute) const {
  return false;
}

bool MarkupAccumulator::ShouldIgnoreElement(const Element& element) const {
  return false;
}

void MarkupAccumulator::AppendElement(StringBuilder& result,
                                      const Element& element,
                                      Namespaces* namespaces) {
  AppendOpenTag(result, element, namespaces);

  AttributeCollection attributes = element.Attributes();
  for (const auto& attribute : attributes) {
    if (!ShouldIgnoreAttribute(element, attribute))
      AppendAttribute(result, element, attribute, namespaces);
  }

  // Give an opportunity to subclasses to add their own attributes.
  AppendCustomAttributes(result, element, namespaces);

  AppendCloseTag(result, element);
}

void MarkupAccumulator::AppendOpenTag(StringBuilder& result,
                                      const Element& element,
                                      Namespaces* namespaces) {
  formatter_.AppendOpenTag(result, element, namespaces);
}

void MarkupAccumulator::AppendCloseTag(StringBuilder& result,
                                       const Element& element) {
  formatter_.AppendCloseTag(result, element);
}

void MarkupAccumulator::AppendAttribute(StringBuilder& result,
                                        const Element& element,
                                        const Attribute& attribute,
                                        Namespaces* namespaces) {
  formatter_.AppendAttribute(result, element, attribute, namespaces);
}

EntityMask MarkupAccumulator::EntityMaskForText(const Text& text) const {
  return formatter_.EntityMaskForText(text);
}

bool MarkupAccumulator::SerializeAsHTMLDocument(const Node& node) const {
  return formatter_.SerializeAsHTMLDocument(node);
}

std::pair<Node*, Element*> MarkupAccumulator::GetAuxiliaryDOMTree(
    const Element& element) const {
  return std::pair<Node*, Element*>();
}

template <typename Strategy>
static void SerializeNodesWithNamespaces(MarkupAccumulator& accumulator,
                                         Node& target_node,
                                         EChildrenOnly children_only,
                                         const Namespaces* namespaces) {
  if (target_node.IsElementNode() &&
      accumulator.ShouldIgnoreElement(ToElement(target_node))) {
    return;
  }

  Namespaces namespace_hash;
  if (namespaces)
    namespace_hash = *namespaces;

  if (!children_only)
    accumulator.AppendStartTag(target_node, &namespace_hash);

  if (!(accumulator.SerializeAsHTMLDocument(target_node) &&
        ElementCannotHaveEndTag(target_node))) {
    Node* current = isHTMLTemplateElement(target_node)
                        ? Strategy::FirstChild(
                              *toHTMLTemplateElement(target_node).content())
                        : Strategy::FirstChild(target_node);
    for (; current; current = Strategy::NextSibling(*current))
      SerializeNodesWithNamespaces<Strategy>(accumulator, *current,
                                             kIncludeNode, &namespace_hash);

    // Traverses other DOM tree, i.e., shadow tree.
    if (target_node.IsElementNode()) {
      std::pair<Node*, Element*> auxiliary_pair =
          accumulator.GetAuxiliaryDOMTree(ToElement(target_node));
      Node* auxiliary_tree = auxiliary_pair.first;
      Element* enclosing_element = auxiliary_pair.second;
      if (auxiliary_tree) {
        if (auxiliary_pair.second)
          accumulator.AppendStartTag(*enclosing_element);
        current = Strategy::FirstChild(*auxiliary_tree);
        for (; current; current = Strategy::NextSibling(*current)) {
          SerializeNodesWithNamespaces<Strategy>(accumulator, *current,
                                                 kIncludeNode, &namespace_hash);
        }
        if (enclosing_element)
          accumulator.AppendEndTag(*enclosing_element);
      }
    }
  }

  if ((!children_only && target_node.IsElementNode()) &&
      !(accumulator.SerializeAsHTMLDocument(target_node) &&
        ElementCannotHaveEndTag(target_node)))
    accumulator.AppendEndTag(ToElement(target_node));
}

template <typename Strategy>
String SerializeNodes(MarkupAccumulator& accumulator,
                      Node& target_node,
                      EChildrenOnly children_only) {
  Namespaces* namespaces = nullptr;
  Namespaces namespace_hash;
  if (!accumulator.SerializeAsHTMLDocument(target_node)) {
    // Add pre-bound namespaces for XML fragments.
    namespace_hash.Set(g_xml_atom, XMLNames::xmlNamespaceURI);
    namespaces = &namespace_hash;
  }

  SerializeNodesWithNamespaces<Strategy>(accumulator, target_node,
                                         children_only, namespaces);
  return accumulator.ToString();
}

template String SerializeNodes<EditingStrategy>(MarkupAccumulator&,
                                                Node&,
                                                EChildrenOnly);

}  // namespace blink
