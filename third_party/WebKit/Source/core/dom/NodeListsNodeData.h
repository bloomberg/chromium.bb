/*
 * Copyright (C) 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Smith <catfish.man@gmail.com>
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

#ifndef NodeListsNodeData_h
#define NodeListsNodeData_h

#include "core/dom/ChildNodeList.h"
#include "core/dom/EmptyNodeList.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/TagCollection.h"
#include "core/html/CollectionType.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringHash.h"

namespace blink {

class NodeListsNodeData final : public GarbageCollected<NodeListsNodeData> {
  WTF_MAKE_NONCOPYABLE(NodeListsNodeData);

 public:
  ChildNodeList* GetChildNodeList(ContainerNode& node) {
    DCHECK(!child_node_list_ || node == child_node_list_->VirtualOwnerNode());
    return ToChildNodeList(child_node_list_);
  }

  ChildNodeList* EnsureChildNodeList(ContainerNode& node) {
    DCHECK(ThreadState::Current()->IsGCForbidden());
    if (child_node_list_)
      return ToChildNodeList(child_node_list_);
    ChildNodeList* list = ChildNodeList::Create(node);
    child_node_list_ = list;
    return list;
  }

  EmptyNodeList* EnsureEmptyChildNodeList(Node& node) {
    DCHECK(ThreadState::Current()->IsGCForbidden());
    if (child_node_list_)
      return ToEmptyNodeList(child_node_list_);
    EmptyNodeList* list = EmptyNodeList::Create(node);
    child_node_list_ = list;
    return list;
  }

  using NamedNodeListKey = std::pair<CollectionType, AtomicString>;
  struct NodeListAtomicCacheMapEntryHash {
    STATIC_ONLY(NodeListAtomicCacheMapEntryHash);
    static unsigned GetHash(const NamedNodeListKey& entry) {
      return DefaultHash<AtomicString>::Hash::GetHash(entry.second) +
             entry.first;
    }
    static bool Equal(const NamedNodeListKey& a, const NamedNodeListKey& b) {
      return a == b;
    }
    static const bool safe_to_compare_to_empty_or_deleted =
        DefaultHash<AtomicString>::Hash::safe_to_compare_to_empty_or_deleted;
  };

  typedef HeapHashMap<NamedNodeListKey,
                      TraceWrapperMember<LiveNodeListBase>,
                      NodeListAtomicCacheMapEntryHash>
      NodeListAtomicNameCacheMap;
  typedef HeapHashMap<QualifiedName, TraceWrapperMember<TagCollectionNS>>
      TagCollectionNSCache;

  template <typename T>
  T* AddCache(ContainerNode& node,
              CollectionType collection_type,
              const AtomicString& name) {
    DCHECK(ThreadState::Current()->IsGCForbidden());
    NodeListAtomicNameCacheMap::AddResult result = atomic_name_caches_.insert(
        std::make_pair(collection_type, name), nullptr);
    if (!result.is_new_entry) {
      return static_cast<T*>(result.stored_value->value.Get());
    }

    T* list = T::Create(node, collection_type, name);
    result.stored_value->value = list;
    return list;
  }

  template <typename T>
  T* AddCache(ContainerNode& node, CollectionType collection_type) {
    DCHECK(ThreadState::Current()->IsGCForbidden());
    NodeListAtomicNameCacheMap::AddResult result = atomic_name_caches_.insert(
        NamedNodeListKey(collection_type, g_star_atom), nullptr);
    if (!result.is_new_entry) {
      return static_cast<T*>(result.stored_value->value.Get());
    }

    T* list = T::Create(node, collection_type);
    result.stored_value->value = list;
    return list;
  }

  template <typename T>
  T* Cached(CollectionType collection_type) {
    return static_cast<T*>(
        atomic_name_caches_.at(NamedNodeListKey(collection_type, g_star_atom)));
  }

  TagCollectionNS* AddCache(ContainerNode& node,
                            const AtomicString& namespace_uri,
                            const AtomicString& local_name) {
    DCHECK(ThreadState::Current()->IsGCForbidden());
    QualifiedName name(g_null_atom, local_name, namespace_uri);
    TagCollectionNSCache::AddResult result =
        tag_collection_ns_caches_.insert(name, nullptr);
    if (!result.is_new_entry)
      return result.stored_value->value;

    TagCollectionNS* list =
        TagCollectionNS::Create(node, namespace_uri, local_name);
    result.stored_value->value = list;
    return list;
  }

  static NodeListsNodeData* Create() { return new NodeListsNodeData; }

  void InvalidateCaches(const QualifiedName* attr_name = 0);

  bool IsEmpty() const {
    return !child_node_list_ && atomic_name_caches_.IsEmpty() &&
           tag_collection_ns_caches_.IsEmpty();
  }

  void AdoptTreeScope() { InvalidateCaches(); }

  void AdoptDocument(Document& old_document, Document& new_document) {
    DCHECK_NE(old_document, new_document);

    NodeListAtomicNameCacheMap::const_iterator atomic_name_cache_end =
        atomic_name_caches_.end();
    for (NodeListAtomicNameCacheMap::const_iterator it =
             atomic_name_caches_.begin();
         it != atomic_name_cache_end; ++it) {
      LiveNodeListBase* list = it->value;
      list->DidMoveToDocument(old_document, new_document);
    }

    TagCollectionNSCache::const_iterator tag_end =
        tag_collection_ns_caches_.end();
    for (TagCollectionNSCache::const_iterator it =
             tag_collection_ns_caches_.begin();
         it != tag_end; ++it) {
      LiveNodeListBase* list = it->value;
      DCHECK(!list->IsRootedAtTreeScope());
      list->DidMoveToDocument(old_document, new_document);
    }
  }

  void Trace(blink::Visitor*);
  void TraceWrappers(const ScriptWrappableVisitor*) const;

 private:
  NodeListsNodeData() : child_node_list_(nullptr) {}

  // Can be a ChildNodeList or an EmptyNodeList.
  TraceWrapperMember<NodeList> child_node_list_;
  NodeListAtomicNameCacheMap atomic_name_caches_;
  TagCollectionNSCache tag_collection_ns_caches_;
};

DEFINE_TRAIT_FOR_TRACE_WRAPPERS(NodeListsNodeData);

template <typename Collection>
inline Collection* ContainerNode::EnsureCachedCollection(CollectionType type) {
  ThreadState::MainThreadGCForbiddenScope gc_forbidden;
  return EnsureNodeLists().AddCache<Collection>(*this, type);
}

template <typename Collection>
inline Collection* ContainerNode::EnsureCachedCollection(
    CollectionType type,
    const AtomicString& name) {
  ThreadState::MainThreadGCForbiddenScope gc_forbidden;
  return EnsureNodeLists().AddCache<Collection>(*this, type, name);
}

template <typename Collection>
inline Collection* ContainerNode::EnsureCachedCollection(
    CollectionType type,
    const AtomicString& namespace_uri,
    const AtomicString& local_name) {
  DCHECK_EQ(type, kTagCollectionNSType);
  ThreadState::MainThreadGCForbiddenScope gc_forbidden;
  return EnsureNodeLists().AddCache(*this, namespace_uri, local_name);
}

template <typename Collection>
inline Collection* ContainerNode::CachedCollection(CollectionType type) {
  NodeListsNodeData* node_lists = NodeLists();
  return node_lists ? node_lists->Cached<Collection>(type) : nullptr;
}

}  // namespace blink

#endif  // NodeListsNodeData_h
