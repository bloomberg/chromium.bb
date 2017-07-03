/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef TreeOrderedMap_h
#define TreeOrderedMap_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/AtomicStringHash.h"
#include "platform/wtf/text/StringImpl.h"

namespace blink {

class Element;
class HTMLSlotElement;
class TreeScope;

class TreeOrderedMap : public GarbageCollected<TreeOrderedMap> {
 public:
  static TreeOrderedMap* Create();

  void Add(const AtomicString&, Element*);
  void Remove(const AtomicString&, Element*);

  bool Contains(const AtomicString&) const;
  bool ContainsMultiple(const AtomicString&) const;
  // concrete instantiations of the get<>() method template
  Element* GetElementById(const AtomicString&, const TreeScope&) const;
  const HeapVector<Member<Element>>& GetAllElementsById(const AtomicString&,
                                                        const TreeScope&) const;
  Element* GetElementByMapName(const AtomicString&, const TreeScope&) const;
  HTMLSlotElement* GetSlotByName(const AtomicString&, const TreeScope&) const;
  // Don't use this unless the caller can know the internal state of
  // TreeOrderedMap exactly.
  Element* GetCachedFirstElementWithoutAccessingNodeTree(const AtomicString&);

  DECLARE_TRACE();

#if DCHECK_IS_ON()
  // While removing a ContainerNode, ID lookups won't be precise should the tree
  // have elements with duplicate IDs contained in the element being removed.
  // Rare trees, but ID lookups may legitimately fail across such removals;
  // this scope object informs TreeOrderedMaps about the transitory state of the
  // underlying tree.
  class RemoveScope {
    STACK_ALLOCATED();

   public:
    RemoveScope();
    ~RemoveScope();
  };
#else
  class RemoveScope {
    STACK_ALLOCATED();

   public:
    RemoveScope() {}
    ~RemoveScope() {}
  };
#endif

 private:
  TreeOrderedMap();

  template <bool keyMatches(const AtomicString&, const Element&)>
  Element* Get(const AtomicString&, const TreeScope&) const;

  class MapEntry : public GarbageCollected<MapEntry> {
   public:
    explicit MapEntry(Element* first_element)
        : element(first_element), count(1) {}

    DECLARE_TRACE();

    Member<Element> element;
    unsigned count;
    HeapVector<Member<Element>> ordered_list;
  };

  using Map = HeapHashMap<AtomicString, Member<MapEntry>>;

  mutable Map map_;
};

inline bool TreeOrderedMap::Contains(const AtomicString& id) const {
  return map_.Contains(id);
}

inline bool TreeOrderedMap::ContainsMultiple(const AtomicString& id) const {
  Map::const_iterator it = map_.find(id);
  return it != map_.end() && it->value->count > 1;
}

}  // namespace blink

#endif  // TreeOrderedMap_h
