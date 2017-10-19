// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NthIndexCache_h
#define NthIndexCache_h

#include "core/CoreExport.h"
#include "core/dom/Element.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashMap.h"

namespace blink {

class Document;

class CORE_EXPORT NthIndexData final : public GarbageCollected<NthIndexData> {
  WTF_MAKE_NONCOPYABLE(NthIndexData);

 public:
  NthIndexData(ContainerNode&);
  NthIndexData(ContainerNode&, const QualifiedName& type);

  unsigned NthIndex(Element&) const;
  unsigned NthLastIndex(Element&) const;
  unsigned NthOfTypeIndex(Element&) const;
  unsigned NthLastOfTypeIndex(Element&) const;

  void Trace(blink::Visitor*);

 private:
  HeapHashMap<Member<Element>, unsigned> element_index_map_;
  unsigned count_ = 0;
};

class CORE_EXPORT NthIndexCache final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(NthIndexCache);

 public:
  explicit NthIndexCache(Document&);
  ~NthIndexCache();

  static unsigned NthChildIndex(Element&);
  static unsigned NthLastChildIndex(Element&);
  static unsigned NthOfTypeIndex(Element&);
  static unsigned NthLastOfTypeIndex(Element&);

 private:
  using IndexByType = HeapHashMap<String, Member<NthIndexData>>;
  using ParentMap = HeapHashMap<Member<Node>, Member<NthIndexData>>;
  using ParentMapForType = HeapHashMap<Member<Node>, Member<IndexByType>>;

  void CacheNthIndexDataForParent(Element&);
  void CacheNthOfTypeIndexDataForParent(Element&);
  IndexByType& EnsureTypeIndexMap(ContainerNode&);
  NthIndexData* NthTypeIndexDataForParent(Element&) const;

  Member<Document> document_;
  Member<ParentMap> parent_map_;
  Member<ParentMapForType> parent_map_for_type_;

#if DCHECK_IS_ON()
  uint64_t dom_tree_version_;
#endif
};

}  // namespace blink

#endif  // NthIndexCache_h
