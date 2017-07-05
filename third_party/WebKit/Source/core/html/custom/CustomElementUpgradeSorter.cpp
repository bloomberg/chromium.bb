// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/custom/CustomElementUpgradeSorter.h"

#include "core/dom/Element.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/Node.h"
#include "core/dom/ShadowRoot.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/imports/HTMLImportChild.h"
#include "core/html/imports/HTMLImportLoader.h"

namespace blink {

CustomElementUpgradeSorter::CustomElementUpgradeSorter()
    : elements_(new HeapHashSet<Member<Element>>()),
      parent_child_map_(new ParentChildMap()) {}

static HTMLLinkElement* GetLinkElementForImport(const Document& import) {
  if (HTMLImportLoader* loader = import.ImportLoader())
    return loader->FirstImport()->Link();
  return nullptr;
}

CustomElementUpgradeSorter::AddResult
CustomElementUpgradeSorter::AddToParentChildMap(Node* parent, Node* child) {
  ParentChildMap::AddResult result = parent_child_map_->insert(parent, nullptr);
  if (!result.is_new_entry) {
    result.stored_value->value->insert(child);
    // The entry for the parent exists; so must its parents.
    return kParentAlreadyExistsInMap;
  }

  ChildSet* child_set = new ChildSet();
  child_set->insert(child);
  result.stored_value->value = child_set;
  return kParentAddedToMap;
}

void CustomElementUpgradeSorter::Add(Element* element) {
  elements_->insert(element);

  for (Node *n = element, *parent = n->ParentOrShadowHostNode(); parent;
       n = parent, parent = parent->ParentOrShadowHostNode()) {
    if (AddToParentChildMap(parent, n) == kParentAlreadyExistsInMap)
      break;

    // Create parent-child link between <link rel="import"> and its imported
    // document so that the content of the imported document be visited as if
    // the imported document were inserted in the link element.
    if (parent->IsDocumentNode()) {
      Element* link = GetLinkElementForImport(*ToDocument(parent));
      if (!link ||
          AddToParentChildMap(link, parent) == kParentAlreadyExistsInMap)
        break;
      parent = link;
    }
  }
}

void CustomElementUpgradeSorter::Visit(HeapVector<Member<Element>>* result,
                                       ChildSet& children,
                                       const ChildSet::iterator& it) {
  if (it == children.end())
    return;
  if (it->Get()->IsElementNode() && elements_->Contains(ToElement(*it)))
    result->push_back(ToElement(*it));
  Sorted(result, *it);
  children.erase(it);
}

void CustomElementUpgradeSorter::Sorted(HeapVector<Member<Element>>* result,
                                        Node* parent) {
  ParentChildMap::iterator children_iterator = parent_child_map_->find(parent);
  if (children_iterator == parent_child_map_->end())
    return;

  ChildSet* children = children_iterator->value.Get();

  if (children->size() == 1) {
    Visit(result, *children, children->begin());
    return;
  }

  // TODO(dominicc): When custom elements are used in UA shadow
  // roots, expand this to include UA shadow roots.
  ShadowRoot* shadow_root =
      parent->IsElementNode() ? ToElement(parent)->AuthorShadowRoot() : nullptr;
  if (shadow_root)
    Visit(result, *children, children->find(shadow_root));

  for (Element* e = ElementTraversal::FirstChild(*parent);
       e && children->size() > 1; e = ElementTraversal::NextSibling(*e)) {
    Visit(result, *children, children->find(e));
  }

  if (children->size() == 1)
    Visit(result, *children, children->begin());

  DCHECK(children->IsEmpty());
}

}  // namespace blink
