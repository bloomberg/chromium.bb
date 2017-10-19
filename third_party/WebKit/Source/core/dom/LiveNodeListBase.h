/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef LiveNodeListBase_h
#define LiveNodeListBase_h

#include "core/CoreExport.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/html/CollectionType.h"
#include "core/html_names.h"
#include "platform/heap/Handle.h"

namespace blink {

enum class NodeListRootType {
  kNode,
  kTreeScope,
};

class CORE_EXPORT LiveNodeListBase : public GarbageCollectedMixin {
 public:
  LiveNodeListBase(ContainerNode& owner_node,
                   NodeListRootType root_type,
                   NodeListInvalidationType invalidation_type,
                   CollectionType collection_type)
      : owner_node_(owner_node),
        root_type_(static_cast<unsigned>(root_type)),
        invalidation_type_(invalidation_type),
        collection_type_(collection_type) {
    DCHECK_EQ(root_type_, static_cast<unsigned>(root_type));
    DCHECK_EQ(invalidation_type_, static_cast<unsigned>(invalidation_type));
    DCHECK_EQ(collection_type_, static_cast<unsigned>(collection_type));
  }

  virtual ~LiveNodeListBase() {}

  ContainerNode& RootNode() const;

  void DidMoveToDocument(Document& old_document, Document& new_document);
  ALWAYS_INLINE bool IsRootedAtTreeScope() const {
    return root_type_ == static_cast<unsigned>(NodeListRootType::kTreeScope);
  }
  ALWAYS_INLINE NodeListInvalidationType InvalidationType() const {
    return static_cast<NodeListInvalidationType>(invalidation_type_);
  }
  ALWAYS_INLINE CollectionType GetType() const {
    return static_cast<CollectionType>(collection_type_);
  }
  ContainerNode& ownerNode() const { return *owner_node_; }

  virtual void InvalidateCache(Document* old_document = 0) const = 0;
  void InvalidateCacheForAttribute(const QualifiedName*) const;

  static bool ShouldInvalidateTypeOnAttributeChange(NodeListInvalidationType,
                                                    const QualifiedName&);

 protected:
  Document& GetDocument() const { return owner_node_->GetDocument(); }

  ALWAYS_INLINE NodeListRootType RootType() const {
    return static_cast<NodeListRootType>(root_type_);
  }

  template <typename MatchFunc>
  static Element* TraverseMatchingElementsForwardToOffset(
      Element& current_element,
      const ContainerNode* stay_within,
      unsigned offset,
      unsigned& current_offset,
      MatchFunc);
  template <typename MatchFunc>
  static Element* TraverseMatchingElementsBackwardToOffset(
      Element& current_element,
      const ContainerNode* stay_within,
      unsigned offset,
      unsigned& current_offset,
      MatchFunc);

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(owner_node_); }

 private:
  Member<ContainerNode> owner_node_;  // Cannot be null.
  const unsigned root_type_ : 1;
  const unsigned invalidation_type_ : 4;
  const unsigned collection_type_ : 5;
};

ALWAYS_INLINE bool LiveNodeListBase::ShouldInvalidateTypeOnAttributeChange(
    NodeListInvalidationType type,
    const QualifiedName& attr_name) {
  switch (type) {
    case kInvalidateOnClassAttrChange:
      return attr_name == HTMLNames::classAttr;
    case kInvalidateOnNameAttrChange:
      return attr_name == HTMLNames::nameAttr;
    case kInvalidateOnIdNameAttrChange:
      return attr_name == HTMLNames::idAttr || attr_name == HTMLNames::nameAttr;
    case kInvalidateOnForAttrChange:
      return attr_name == HTMLNames::forAttr;
    case kInvalidateForFormControls:
      return attr_name == HTMLNames::nameAttr ||
             attr_name == HTMLNames::idAttr ||
             attr_name == HTMLNames::forAttr ||
             attr_name == HTMLNames::formAttr ||
             attr_name == HTMLNames::typeAttr;
    case kInvalidateOnHRefAttrChange:
      return attr_name == HTMLNames::hrefAttr;
    case kDoNotInvalidateOnAttributeChanges:
      return false;
    case kInvalidateOnAnyAttrChange:
      return true;
  }
  return false;
}

template <typename MatchFunc>
Element* LiveNodeListBase::TraverseMatchingElementsForwardToOffset(
    Element& current_element,
    const ContainerNode* stay_within,
    unsigned offset,
    unsigned& current_offset,
    MatchFunc is_match) {
  DCHECK_LT(current_offset, offset);
  for (Element* next =
           ElementTraversal::Next(current_element, stay_within, is_match);
       next; next = ElementTraversal::Next(*next, stay_within, is_match)) {
    if (++current_offset == offset)
      return next;
  }
  return nullptr;
}

template <typename MatchFunc>
Element* LiveNodeListBase::TraverseMatchingElementsBackwardToOffset(
    Element& current_element,
    const ContainerNode* stay_within,
    unsigned offset,
    unsigned& current_offset,
    MatchFunc is_match) {
  DCHECK_GT(current_offset, offset);
  for (Element* previous =
           ElementTraversal::Previous(current_element, stay_within, is_match);
       previous; previous = ElementTraversal::Previous(*previous, stay_within,
                                                       is_match)) {
    if (--current_offset == offset)
      return previous;
  }
  return nullptr;
}

}  // namespace blink

#endif  // LiveNodeListBase_h
