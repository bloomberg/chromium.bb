/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef LiveNodeList_h
#define LiveNodeList_h

#include "core/CoreExport.h"
#include "core/dom/LiveNodeListBase.h"
#include "core/dom/NodeList.h"
#include "core/html/CollectionItemsCache.h"
#include "core/html/CollectionType.h"
#include "platform/heap/Handle.h"

namespace blink {

class Element;

class CORE_EXPORT LiveNodeList : public NodeList, public LiveNodeListBase {
  USING_GARBAGE_COLLECTED_MIXIN(LiveNodeList);

 public:
  LiveNodeList(ContainerNode& owner_node,
               CollectionType collection_type,
               NodeListInvalidationType invalidation_type,
               NodeListRootType root_type = NodeListRootType::kNode)
      : LiveNodeListBase(owner_node,
                         root_type,
                         invalidation_type,
                         collection_type) {
    // Keep this in the child class because |registerNodeList| requires wrapper
    // tracing and potentially calls virtual methods which is not allowed in a
    // base class constructor.
    GetDocument().RegisterNodeList(this);
  }

  unsigned length() const final;
  Element* item(unsigned offset) const final;
  virtual bool ElementMatches(const Element&) const = 0;

  void InvalidateCache(Document* old_document = 0) const final;
  void InvalidateCacheForAttribute(const QualifiedName*) const;

  // Collection IndexCache API.
  bool CanTraverseBackward() const { return true; }
  Element* TraverseToFirst() const;
  Element* TraverseToLast() const;
  Element* TraverseForwardToOffset(unsigned offset,
                                   Element& current_node,
                                   unsigned& current_offset) const;
  Element* TraverseBackwardToOffset(unsigned offset,
                                    Element& current_node,
                                    unsigned& current_offset) const;

  virtual void Trace(blink::Visitor*);

 private:
  Node* VirtualOwnerNode() const final;

  mutable CollectionItemsCache<LiveNodeList, Element> collection_items_cache_;
};

DEFINE_TYPE_CASTS(LiveNodeList,
                  LiveNodeListBase,
                  list,
                  IsLiveNodeListType(list->GetType()),
                  IsLiveNodeListType(list.GetType()));

inline void LiveNodeList::InvalidateCacheForAttribute(
    const QualifiedName* attr_name) const {
  if (!attr_name ||
      ShouldInvalidateTypeOnAttributeChange(InvalidationType(), *attr_name))
    InvalidateCache();
}

}  // namespace blink

#endif  // LiveNodeList_h
