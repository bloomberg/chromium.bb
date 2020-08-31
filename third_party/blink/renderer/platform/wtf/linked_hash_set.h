/*
 * Copyright (C) 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2011, Benjamin Poulain <ikipou@gmail.com>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LINKED_HASH_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_LINKED_HASH_SET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partition_allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "third_party/blink/renderer/platform/wtf/vector_backed_linked_list.h"

namespace WTF {

// LinkedHashSet provides a Set interface like HashSet, but also has a
// predictable iteration order. It has O(1) insertion, removal, and test for
// containership. It maintains a linked list through its contents such that
// iterating it yields values in the order in which they were inserted.
//
// LinkedHashSet iterators are invalidated by mutation of the set. This means,
// for example, that you cannot modify the container while iterating
// over it (this will DCHECK in debug). Instead, you should either copy the
// entries to a vector before iterating, or maintain a separate list of pending
// updates.
//
// Unlike ListHashSet, this container supports WeakMember<T>.
template <typename Value,
          typename HashFunctions,
          typename HashTraits,
          typename Allocator>
class LinkedHashSet;

template <typename LinkedHashSet>
class LinkedHashSetConstIterator;
template <typename LinkedHashSet>
class LinkedHashSetConstReverseIterator;

template <typename Value, typename HashFunctions, typename TraitsArg>
struct LinkedHashSetTranslator;
template <typename Value>
struct LinkedHashSetExtractor;
template <typename Value, typename ValueTraits, typename Allocator>
struct LinkedHashSetTraits;
class LinkedHashSetNodeBase;

class LinkedHashSetNodeBasePointer {
 public:
  LinkedHashSetNodeBasePointer(LinkedHashSetNodeBase* node) : node_(node) {}

  LinkedHashSetNodeBasePointer& operator=(
      const LinkedHashSetNodeBasePointer& other) {
    SetSafe(other);
    return *this;
  }

  LinkedHashSetNodeBasePointer& operator=(LinkedHashSetNodeBase* other) {
    SetSafe(other);
    return *this;
  }

  LinkedHashSetNodeBasePointer& operator=(std::nullptr_t) {
    SetSafe(nullptr);
    return *this;
  }

  LinkedHashSetNodeBase* Get() const { return node_; }
  explicit operator bool() const { return Get(); }
  operator LinkedHashSetNodeBase*() const { return Get(); }
  LinkedHashSetNodeBase* operator->() const { return Get(); }
  LinkedHashSetNodeBase& operator*() const { return *Get(); }

 private:
  void SetSafe(LinkedHashSetNodeBase* node) {
    AsAtomicPtr(&node_)->store(node, std::memory_order_relaxed);
  }

  LinkedHashSetNodeBase* node_ = nullptr;
};

class LinkedHashSetNodeBase {
  DISALLOW_NEW();

 public:
  LinkedHashSetNodeBase() : prev_(this), next_(this) {}

  NO_SANITIZE_ADDRESS
  void Unlink() {
    if (!next_)
      return;
    DCHECK(prev_);
    {
      AsanUnpoisonScope unpoison_scope(next_, sizeof(LinkedHashSetNodeBase));
      DCHECK(next_->prev_ == this);
      next_->prev_ = prev_;
    }
    {
      AsanUnpoisonScope unpoison_scope(prev_, sizeof(LinkedHashSetNodeBase));
      DCHECK(prev_->next_ == this);
      prev_->next_ = next_;
    }
  }

  ~LinkedHashSetNodeBase() { Unlink(); }

  void InsertBefore(LinkedHashSetNodeBase& other) {
    other.next_ = this;
    other.prev_ = prev_;
    prev_->next_ = &other;
    prev_ = &other;
    DCHECK(other.next_);
    DCHECK(other.prev_);
    DCHECK(next_);
    DCHECK(prev_);
  }

  void InsertAfter(LinkedHashSetNodeBase& other) {
    other.prev_ = this;
    other.next_ = next_;
    next_->prev_ = &other;
    next_ = &other;
    DCHECK(other.next_);
    DCHECK(other.prev_);
    DCHECK(next_);
    DCHECK(prev_);
  }

  LinkedHashSetNodeBase(LinkedHashSetNodeBase* prev,
                        LinkedHashSetNodeBase* next)
      : prev_(prev), next_(next) {
    DCHECK((prev && next) || (!prev && !next));
  }

  LinkedHashSetNodeBasePointer prev_;
  LinkedHashSetNodeBasePointer next_;

 protected:
  // If we take a copy of a node we can't copy the next and prev pointers,
  // since they point to something that does not point at us. This is used
  // inside the shouldExpand() "if" in HashTable::add.
  LinkedHashSetNodeBase(const LinkedHashSetNodeBase& other)
      : prev_(nullptr), next_(nullptr) {}

  LinkedHashSetNodeBase(LinkedHashSetNodeBase&& other)
      : prev_(other.prev_), next_(other.next_) {
    other.prev_ = nullptr;
    other.next_ = nullptr;
    if (next_) {
      prev_->next_ = this;
      next_->prev_ = this;
    }
  }

 private:
  // Should not be used.
  LinkedHashSetNodeBase& operator=(const LinkedHashSetNodeBase& other) = delete;
};

template <typename ValueArg>
class LinkedHashSetNode : public LinkedHashSetNodeBase {
  DISALLOW_NEW();

 public:
  LinkedHashSetNode(const ValueArg& value,
                    LinkedHashSetNodeBase* prev,
                    LinkedHashSetNodeBase* next)
      : LinkedHashSetNodeBase(prev, next), value_(value) {}

  LinkedHashSetNode(ValueArg&& value,
                    LinkedHashSetNodeBase* prev,
                    LinkedHashSetNodeBase* next)
      : LinkedHashSetNodeBase(prev, next), value_(std::move(value)) {}

  LinkedHashSetNode(LinkedHashSetNode&& other)
      : LinkedHashSetNodeBase(std::move(other)),
        value_(std::move(other.value_)) {}

  ValueArg value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LinkedHashSetNode);
};

template <typename T>
struct IsWeak<LinkedHashSetNode<T>>
    : std::integral_constant<bool, IsWeak<T>::value> {};

template <typename ValueArg,
          typename HashFunctions = typename DefaultHash<ValueArg>::Hash,
          typename TraitsArg = HashTraits<ValueArg>,
          typename Allocator = PartitionAllocator>
class LinkedHashSet {
  USE_ALLOCATOR(LinkedHashSet, Allocator);

 private:
  typedef ValueArg Value;
  typedef TraitsArg Traits;
  typedef LinkedHashSetNode<Value> Node;
  typedef LinkedHashSetNodeBase NodeBase;
  typedef LinkedHashSetTranslator<Value, HashFunctions, Traits>
      NodeHashFunctions;
  typedef LinkedHashSetTraits<Value, Traits, Allocator> NodeHashTraits;

  typedef HashTable<Node,
                    Node,
                    IdentityExtractor,
                    NodeHashFunctions,
                    NodeHashTraits,
                    NodeHashTraits,
                    Allocator>
      ImplType;

 public:
  typedef LinkedHashSetConstIterator<LinkedHashSet> iterator;
  typedef LinkedHashSetConstIterator<LinkedHashSet> const_iterator;
  friend class LinkedHashSetConstIterator<LinkedHashSet>;

  typedef LinkedHashSetConstReverseIterator<LinkedHashSet> reverse_iterator;
  typedef LinkedHashSetConstReverseIterator<LinkedHashSet>
      const_reverse_iterator;
  friend class LinkedHashSetConstReverseIterator<LinkedHashSet>;

  struct AddResult final {
    STACK_ALLOCATED();

   public:
    AddResult(const typename ImplType::AddResult& hash_table_add_result)
        : stored_value(&hash_table_add_result.stored_value->value_),
          is_new_entry(hash_table_add_result.is_new_entry) {}

    const Value* stored_value;
    bool is_new_entry;
  };

  typedef typename TraitsArg::PeekInType ValuePeekInType;

  LinkedHashSet();
  LinkedHashSet(const LinkedHashSet&);
  LinkedHashSet(LinkedHashSet&&);
  LinkedHashSet& operator=(const LinkedHashSet&);
  LinkedHashSet& operator=(LinkedHashSet&&);

  // Needs finalization. The anchor needs to unlink itself from the chain.
  ~LinkedHashSet();

  void Swap(LinkedHashSet&);

  unsigned size() const { return impl_.size(); }
  unsigned Capacity() const { return impl_.Capacity(); }
  bool IsEmpty() const { return impl_.IsEmpty(); }

  iterator begin() { return MakeIterator(FirstNode()); }
  iterator end() { return MakeIterator(Anchor()); }
  const_iterator begin() const { return MakeConstIterator(FirstNode()); }
  const_iterator end() const { return MakeConstIterator(Anchor()); }

  reverse_iterator rbegin() { return MakeReverseIterator(LastNode()); }
  reverse_iterator rend() { return MakeReverseIterator(Anchor()); }
  const_reverse_iterator rbegin() const {
    return MakeConstReverseIterator(LastNode());
  }
  const_reverse_iterator rend() const {
    return MakeConstReverseIterator(Anchor());
  }

  Value& front();
  const Value& front() const;
  void RemoveFirst();

  Value& back();
  const Value& back() const;
  void pop_back();

  iterator find(ValuePeekInType);
  const_iterator find(ValuePeekInType) const;
  bool Contains(ValuePeekInType) const;

  // An alternate version of find() that finds the object by hashing and
  // comparing with some other type, to avoid the cost of type conversion.
  // The HashTranslator interface is defined in HashSet.
  template <typename HashTranslator, typename T>
  iterator Find(const T&);
  template <typename HashTranslator, typename T>
  const_iterator Find(const T&) const;
  template <typename HashTranslator, typename T>
  bool Contains(const T&) const;

  // The return value of insert is a pair of a pointer to the stored value,
  // and a bool that is true if an new entry was added.
  template <typename IncomingValueType>
  AddResult insert(IncomingValueType&&);

  // Add the value to the end of the collection. If the value was already in
  // the list, it is moved to the end.
  template <typename IncomingValueType>
  AddResult AppendOrMoveToLast(IncomingValueType&&);

  // Add the value to the beginning of the collection. If the value was already
  // in the list, it is moved to the beginning.
  template <typename IncomingValueType>
  AddResult PrependOrMoveToFirst(IncomingValueType&&);

  template <typename IncomingValueType>
  AddResult InsertBefore(ValuePeekInType before_value,
                         IncomingValueType&& new_value);
  template <typename IncomingValueType>
  AddResult InsertBefore(const_iterator it, IncomingValueType&& new_value) {
    return impl_.template insert<NodeHashFunctions>(
        std::forward<IncomingValueType>(new_value), it.GetNode());
  }

  void erase(ValuePeekInType);
  void erase(const_iterator);
  void clear() { impl_.clear(); }
  template <typename Collection>
  void RemoveAll(const Collection& other) {
    WTF::RemoveAll(*this, other);
  }

  template <typename VisitorDispatcher>
  void Trace(VisitorDispatcher visitor) const {
    if (!NodeHashTraits::kCanTraceConcurrently) {
      if (visitor->DeferredTraceIfConcurrent(
              {this, [](blink::Visitor* visitor, const void* object) {
                 reinterpret_cast<const LinkedHashSet<ValueArg, HashFunctions,
                                                      TraitsArg, Allocator>*>(
                     object)
                     ->Trace(visitor);
               }}))
        return;
    }

    impl_.Trace(visitor);
    // Should the underlying table be moved by GC, register a callback
    // that fixes up the interior pointers that the (Heap)LinkedHashSet keeps.
    const auto* table =
        AsAtomicPtr(&impl_.table_)->load(std::memory_order_relaxed);
    if (table) {
      Allocator::RegisterBackingStoreCallback(
          visitor, table,
          NodeHashTraits::template MoveBackingCallback<ImplType>);
    }
  }

  int64_t Modifications() const { return impl_.Modifications(); }
  void CheckModifications(int64_t mods) const {
    impl_.CheckModifications(mods);
  }

 protected:
  typename ImplType::ValueType** GetBufferSlot() {
    return impl_.GetBufferSlot();
  }

 private:
  Node* Anchor() { return reinterpret_cast<Node*>(&anchor_); }
  const Node* Anchor() const { return reinterpret_cast<const Node*>(&anchor_); }
  Node* FirstNode() { return reinterpret_cast<Node*>(anchor_.next_.Get()); }
  const Node* FirstNode() const {
    return reinterpret_cast<const Node*>(anchor_.next_.Get());
  }
  Node* LastNode() { return reinterpret_cast<Node*>(anchor_.prev_.Get()); }
  const Node* LastNode() const {
    return reinterpret_cast<const Node*>(anchor_.prev_.Get());
  }

  iterator MakeIterator(const Node* position) {
    return iterator(position, this);
  }
  const_iterator MakeConstIterator(const Node* position) const {
    return const_iterator(position, this);
  }
  reverse_iterator MakeReverseIterator(const Node* position) {
    return reverse_iterator(position, this);
  }
  const_reverse_iterator MakeConstReverseIterator(const Node* position) const {
    return const_reverse_iterator(position, this);
  }

  ImplType impl_;
  NodeBase anchor_;
};

template <typename Value, typename HashFunctions, typename TraitsArg>
struct LinkedHashSetTranslator {
  STATIC_ONLY(LinkedHashSetTranslator);
  typedef LinkedHashSetNode<Value> Node;
  typedef LinkedHashSetNodeBase NodeBase;
  typedef typename TraitsArg::PeekInType ValuePeekInType;
  static unsigned GetHash(const Node& node) {
    return HashFunctions::GetHash(node.value_);
  }
  static unsigned GetHash(const ValuePeekInType& key) {
    return HashFunctions::GetHash(key);
  }
  static bool Equal(const Node& a, const ValuePeekInType& b) {
    return HashFunctions::Equal(a.value_, b);
  }
  static bool Equal(const Node& a, const Node& b) {
    return HashFunctions::Equal(a.value_, b.value_);
  }
  template <typename IncomingValueType>
  static void Translate(Node& location,
                        IncomingValueType&& key,
                        NodeBase* anchor) {
    anchor->InsertBefore(location);
    location.value_ = std::forward<IncomingValueType>(key);
  }

  // Empty (or deleted) slots have the next_ pointer set to null, but we
  // don't do anything to the other fields, which may contain junk.
  // Therefore you can't compare a newly constructed empty value with a
  // slot and get the right answer.
  static const bool safe_to_compare_to_empty_or_deleted = false;
};

template <typename Value>
struct LinkedHashSetExtractor {
  STATIC_ONLY(LinkedHashSetExtractor);
  static const Value& Extract(const LinkedHashSetNode<Value>& node) {
    return node.value_;
  }
};

template <typename Value, typename ValueTraitsArg, typename Allocator>
struct LinkedHashSetTraits
    : public SimpleClassHashTraits<LinkedHashSetNode<Value>> {
  STATIC_ONLY(LinkedHashSetTraits);
  using Node = LinkedHashSetNode<Value>;
  using NodeBase = LinkedHashSetNodeBase;
  typedef ValueTraitsArg ValueTraits;

  // The slot is empty when the next_ field is zero so it's safe to zero
  // the backing.
  static const bool kEmptyValueIsZero = ValueTraits::kEmptyValueIsZero;

  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(const Node& node) { return !node.next_; }
  static Node EmptyValue() {
    return Node(ValueTraits::EmptyValue(), nullptr, nullptr);
  }

  static const int kDeletedValue = -1;

  static void ConstructDeletedValue(Node& slot, bool) {
    slot.next_ = reinterpret_cast<Node*>(kDeletedValue);
  }
  static bool IsDeletedValue(const Node& slot) {
    return slot.next_ == reinterpret_cast<Node*>(kDeletedValue);
  }

  // Whether we need to trace and do weak processing depends on the traits of
  // the type inside the node.
  template <typename U = void>
  struct IsTraceableInCollection {
    STATIC_ONLY(IsTraceableInCollection);
    static const bool value =
        ValueTraits::template IsTraceableInCollection<>::value;
  };

  static constexpr bool kHasMovingCallback = true;

  template <typename HashTable, typename Visitor>
  static void RegisterMovingCallback(Visitor* visitor,
                                     typename HashTable::ValueType* allocated) {
    Allocator::RegisterBackingStoreCallback(visitor, allocated,
                                            MoveBackingCallback<HashTable>);
  }

  template <typename HashTable>
  static void MoveBackingCallback(const void* const_from,
                                  const void* const_to,
                                  size_t size) {
    // Note: the hash table move may have been overlapping; linearly scan the
    // entire table and fixup interior pointers into the old region with
    // correspondingly offset ones into the new.
    void* from = const_cast<void*>(const_from);
    void* to = const_cast<void*>(const_to);
    const size_t table_size = size / sizeof(Node);
    Node* table = reinterpret_cast<Node*>(to);
    NodeBase* from_start = reinterpret_cast<NodeBase*>(from);
    NodeBase* from_end =
        reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(from) + size);
    NodeBase* anchor_node = nullptr;
    for (Node* element = table + table_size - 1; element >= table; element--) {
      Node& node = *element;
      if (HashTable::IsEmptyOrDeletedBucket(node))
        continue;
      if (node.next_ >= from_start && node.next_ < from_end) {
        const size_t diff = reinterpret_cast<uintptr_t>(node.next_.Get()) -
                            reinterpret_cast<uintptr_t>(from);
        node.next_ =
            reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
      } else {
        DCHECK(!anchor_node || node.next_ == anchor_node);
        anchor_node = node.next_;
      }
      if (node.prev_ >= from_start && node.prev_ < from_end) {
        const size_t diff = reinterpret_cast<uintptr_t>(node.prev_.Get()) -
                            reinterpret_cast<uintptr_t>(from);
        node.prev_ =
            reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
      } else {
        DCHECK(!anchor_node || node.prev_ == anchor_node);
        anchor_node = node.prev_;
      }
    }
    // During incremental marking, HeapLinkedHashSet object may be marked, but
    // later the mutator can destroy it. The compaction code will execute this
    // callback, but the anchor will have already been unlinked.
    if (!anchor_node) {
      return;
    }
    {
      DCHECK(anchor_node->prev_ >= from_start && anchor_node->prev_ < from_end);
      const size_t diff =
          reinterpret_cast<uintptr_t>(anchor_node->prev_.Get()) -
          reinterpret_cast<uintptr_t>(from);
      anchor_node->prev_ =
          reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
    }
    {
      DCHECK(anchor_node->next_ >= from_start && anchor_node->next_ < from_end);
      const size_t diff =
          reinterpret_cast<uintptr_t>(anchor_node->next_.Get()) -
          reinterpret_cast<uintptr_t>(from);
      anchor_node->next_ =
          reinterpret_cast<NodeBase*>(reinterpret_cast<uintptr_t>(to) + diff);
    }
  }

  static constexpr bool kCanTraceConcurrently =
      ValueTraitsArg::kCanTraceConcurrently;
};

template <typename LinkedHashSetType>
class LinkedHashSetConstIterator {
  DISALLOW_NEW();

 private:
  typedef typename LinkedHashSetType::Node Node;
  typedef typename LinkedHashSetType::Traits Traits;

  typedef const typename LinkedHashSetType::Value& ReferenceType;
  typedef const typename LinkedHashSetType::Value* PointerType;

  Node* GetNode() const {
    return const_cast<Node*>(static_cast<const Node*>(position_));
  }

 protected:
  LinkedHashSetConstIterator(const LinkedHashSetNodeBase* position,
                             const LinkedHashSetType* container)
      : position_(position)
#if DCHECK_IS_ON()
        ,
        container_(container),
        container_modifications_(container->Modifications())
#endif
  {
  }

 public:
  PointerType Get() const {
    CheckModifications();
    return &static_cast<const Node*>(position_)->value_;
  }
  ReferenceType operator*() const { return *Get(); }
  PointerType operator->() const { return Get(); }

  LinkedHashSetConstIterator& operator++() {
    DCHECK(position_);
    CheckModifications();
    position_ = position_->next_;
    return *this;
  }

  LinkedHashSetConstIterator& operator--() {
    DCHECK(position_);
    CheckModifications();
    position_ = position_->prev_;
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  // Comparison.
  bool operator==(const LinkedHashSetConstIterator& other) const {
    return position_ == other.position_;
  }
  bool operator!=(const LinkedHashSetConstIterator& other) const {
    return position_ != other.position_;
  }

 private:
  const LinkedHashSetNodeBase* position_;
#if DCHECK_IS_ON()
  void CheckModifications() const {
    container_->CheckModifications(container_modifications_);
  }
  const LinkedHashSetType* container_;
  int64_t container_modifications_;
#else
  void CheckModifications() const {}
#endif
  template <typename T, typename U, typename V, typename W>
  friend class LinkedHashSet;
};

template <typename LinkedHashSetType>
class LinkedHashSetConstReverseIterator
    : public LinkedHashSetConstIterator<LinkedHashSetType> {
  typedef LinkedHashSetConstIterator<LinkedHashSetType> Superclass;
  typedef typename LinkedHashSetType::Node Node;

 public:
  LinkedHashSetConstReverseIterator(const Node* position,
                                    const LinkedHashSetType* container)
      : Superclass(position, container) {}

  LinkedHashSetConstReverseIterator& operator++() {
    Superclass::operator--();
    return *this;
  }
  LinkedHashSetConstReverseIterator& operator--() {
    Superclass::operator++();
    return *this;
  }

  // Postfix ++ and -- intentionally omitted.

  template <typename T, typename U, typename V, typename W>
  friend class LinkedHashSet;
};

inline void SwapAnchor(LinkedHashSetNodeBase& a, LinkedHashSetNodeBase& b) {
  DCHECK(a.prev_);
  DCHECK(a.next_);
  DCHECK(b.prev_);
  DCHECK(b.next_);
  swap(a.prev_, b.prev_);
  swap(a.next_, b.next_);
  if (b.next_ == &a) {
    DCHECK_EQ(b.prev_, &a);
    b.next_ = &b;
    b.prev_ = &b;
  } else {
    b.next_->prev_ = &b;
    b.prev_->next_ = &b;
  }
  if (a.next_ == &b) {
    DCHECK_EQ(a.prev_, &b);
    a.next_ = &a;
    a.prev_ = &a;
  } else {
    a.next_->prev_ = &a;
    a.prev_->next_ = &a;
  }
}

inline void swap(LinkedHashSetNodeBase& a, LinkedHashSetNodeBase& b) {
  DCHECK_NE(a.next_, &a);
  DCHECK_NE(b.next_, &b);
  swap(a.prev_, b.prev_);
  swap(a.next_, b.next_);
  if (b.next_) {
    b.next_->prev_ = &b;
    b.prev_->next_ = &b;
  }
  if (a.next_) {
    a.next_->prev_ = &a;
    a.prev_->next_ = &a;
  }
}

template <typename T, typename U, typename V, typename Allocator>
inline LinkedHashSet<T, U, V, Allocator>::LinkedHashSet() {
  static_assert(
      Allocator::kIsGarbageCollected ||
          !IsPointerToGarbageCollectedType<T>::value,
      "Cannot put raw pointers to garbage-collected classes into "
      "an off-heap LinkedHashSet. Use HeapLinkedHashSet<Member<T>> instead.");
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>::LinkedHashSet(const LinkedHashSet& other)
    : anchor_() {
  const_iterator end = other.end();
  for (const_iterator it = other.begin(); it != end; ++it)
    insert(*it);
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>::LinkedHashSet(LinkedHashSet&& other)
    : anchor_() {
  Swap(other);
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>& LinkedHashSet<T, U, V, W>::operator=(
    const LinkedHashSet& other) {
  LinkedHashSet tmp(other);
  Swap(tmp);
  return *this;
}

template <typename T, typename U, typename V, typename W>
inline LinkedHashSet<T, U, V, W>& LinkedHashSet<T, U, V, W>::operator=(
    LinkedHashSet&& other) {
  Swap(other);
  return *this;
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::Swap(LinkedHashSet& other) {
  impl_.swap(other.impl_);
  SwapAnchor(anchor_, other.anchor_);
}

template <typename T, typename U, typename V, typename Allocator>
inline LinkedHashSet<T, U, V, Allocator>::~LinkedHashSet() {
  // The destructor of anchor_ will implicitly be called here, which will
  // unlink the anchor from the collection.
}

template <typename T, typename U, typename V, typename W>
inline T& LinkedHashSet<T, U, V, W>::front() {
  DCHECK(!IsEmpty());
  return FirstNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline const T& LinkedHashSet<T, U, V, W>::front() const {
  DCHECK(!IsEmpty());
  return FirstNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::RemoveFirst() {
  DCHECK(!IsEmpty());
  impl_.erase(static_cast<Node*>(anchor_.next_.Get()));
}

template <typename T, typename U, typename V, typename W>
inline T& LinkedHashSet<T, U, V, W>::back() {
  DCHECK(!IsEmpty());
  return LastNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline const T& LinkedHashSet<T, U, V, W>::back() const {
  DCHECK(!IsEmpty());
  return LastNode()->value_;
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::pop_back() {
  DCHECK(!IsEmpty());
  impl_.erase(static_cast<Node*>(anchor_.prev_.Get()));
}

template <typename T, typename U, typename V, typename W>
inline typename LinkedHashSet<T, U, V, W>::iterator
LinkedHashSet<T, U, V, W>::find(ValuePeekInType value) {
  LinkedHashSet::Node* node =
      impl_.template Lookup<LinkedHashSet::NodeHashFunctions, ValuePeekInType>(
          value);
  if (!node)
    return end();
  return MakeIterator(node);
}

template <typename T, typename U, typename V, typename W>
inline typename LinkedHashSet<T, U, V, W>::const_iterator
LinkedHashSet<T, U, V, W>::find(ValuePeekInType value) const {
  const LinkedHashSet::Node* node =
      impl_.template Lookup<LinkedHashSet::NodeHashFunctions, ValuePeekInType>(
          value);
  if (!node)
    return end();
  return MakeConstIterator(node);
}

template <typename Translator>
struct LinkedHashSetTranslatorAdapter {
  STATIC_ONLY(LinkedHashSetTranslatorAdapter);
  template <typename T>
  static unsigned GetHash(const T& key) {
    return Translator::GetHash(key);
  }
  template <typename T, typename U>
  static bool Equal(const T& a, const U& b) {
    return Translator::Equal(a.value_, b);
  }
};

template <typename Value, typename U, typename V, typename W>
template <typename HashTranslator, typename T>
inline typename LinkedHashSet<Value, U, V, W>::iterator
LinkedHashSet<Value, U, V, W>::Find(const T& value) {
  typedef LinkedHashSetTranslatorAdapter<HashTranslator> TranslatedFunctions;
  const LinkedHashSet::Node* node =
      impl_.template Lookup<TranslatedFunctions, const T&>(value);
  if (!node)
    return end();
  return MakeIterator(node);
}

template <typename Value, typename U, typename V, typename W>
template <typename HashTranslator, typename T>
inline typename LinkedHashSet<Value, U, V, W>::const_iterator
LinkedHashSet<Value, U, V, W>::Find(const T& value) const {
  typedef LinkedHashSetTranslatorAdapter<HashTranslator> TranslatedFunctions;
  const LinkedHashSet::Node* node =
      impl_.template Lookup<TranslatedFunctions, const T&>(value);
  if (!node)
    return end();
  return MakeConstIterator(node);
}

template <typename Value, typename U, typename V, typename W>
template <typename HashTranslator, typename T>
inline bool LinkedHashSet<Value, U, V, W>::Contains(const T& value) const {
  return impl_
      .template Contains<LinkedHashSetTranslatorAdapter<HashTranslator>>(value);
}

template <typename T, typename U, typename V, typename W>
inline bool LinkedHashSet<T, U, V, W>::Contains(ValuePeekInType value) const {
  return impl_.template Contains<NodeHashFunctions>(value);
}

template <typename Value,
          typename HashFunctions,
          typename Traits,
          typename Allocator>
template <typename IncomingValueType>
typename LinkedHashSet<Value, HashFunctions, Traits, Allocator>::AddResult
LinkedHashSet<Value, HashFunctions, Traits, Allocator>::insert(
    IncomingValueType&& value) {
  return impl_.template insert<NodeHashFunctions>(
      std::forward<IncomingValueType>(value), &anchor_);
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
typename LinkedHashSet<T, U, V, W>::AddResult
LinkedHashSet<T, U, V, W>::AppendOrMoveToLast(IncomingValueType&& value) {
  typename ImplType::AddResult result =
      impl_.template insert<NodeHashFunctions>(
          std::forward<IncomingValueType>(value), &anchor_);
  Node* node = result.stored_value;
  if (!result.is_new_entry) {
    node->Unlink();
    anchor_.InsertBefore(*node);
  }
  return result;
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
typename LinkedHashSet<T, U, V, W>::AddResult
LinkedHashSet<T, U, V, W>::PrependOrMoveToFirst(IncomingValueType&& value) {
  typename ImplType::AddResult result =
      impl_.template insert<NodeHashFunctions>(
          std::forward<IncomingValueType>(value), anchor_.next_);
  Node* node = result.stored_value;
  if (!result.is_new_entry) {
    node->Unlink();
    anchor_.InsertAfter(*node);
  }
  return result;
}

template <typename T, typename U, typename V, typename W>
template <typename IncomingValueType>
typename LinkedHashSet<T, U, V, W>::AddResult
LinkedHashSet<T, U, V, W>::InsertBefore(ValuePeekInType before_value,
                                        IncomingValueType&& new_value) {
  return InsertBefore(find(before_value),
                      std::forward<IncomingValueType>(new_value));
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::erase(const_iterator it) {
  if (it == end())
    return;
  impl_.erase(it.GetNode());
}

template <typename T, typename U, typename V, typename W>
inline void LinkedHashSet<T, U, V, W>::erase(ValuePeekInType value) {
  erase(find(value));
}

template <typename T, typename Allocator>
inline void swap(LinkedHashSetNode<T>& a, LinkedHashSetNode<T>& b) {
  // The key and value cannot be swapped atomically, and it would be
  // wrong to have a GC when only one was swapped and the other still
  // contained garbage (eg. from a previous use of the same slot).
  // Therefore we forbid a GC until both the key and the value are
  // swapped.
  Allocator::EnterGCForbiddenScope();
  swap(static_cast<LinkedHashSetNodeBase&>(a),
       static_cast<LinkedHashSetNodeBase&>(b));
  swap(a.value_, b.value_);
  Allocator::LeaveGCForbiddenScope();
}

// TODO(keinakashima): replace existing LinkedHashSet with NewLinkedHashSet
// after completion

// This class is still experimental. Do not use this class.

// LinkedHashSet provides a Set interface like HashSet, but also has a
// predictable iteration order. It has O(1) insertion, removal, and test for
// containership. It maintains a linked list through its contents such that
// iterating it yields values in the order in which they were inserted.
// The linked list is implementing in a vector (with links being indexes instead
// of pointers), to simplify the move of backing during GC compaction.

// TODO(keinakashima): implement NewLinkedHashTraits (now we cannot insert
// deleted/empty value) and add it to template parameter

template <typename ValueArg, typename Allocator = PartitionAllocator>
class NewLinkedHashSet {
  USE_ALLOCATOR(NewLinkedHashSet, Allocator);

 private:
  using Value = ValueArg;
  using Map = HashMap<Value,
                      wtf_size_t,
                      typename DefaultHash<Value>::Hash,
                      HashTraits<Value>,
                      HashTraits<wtf_size_t>,
                      Allocator>;
  using ListType = VectorBackedLinkedList<Value, Allocator>;

 public:
  using iterator = typename ListType::const_iterator;
  using reverse_iterator = typename ListType::const_reverse_iterator;
  using const_iterator = typename ListType::const_iterator;
  using const_reverse_iterator = typename ListType::const_reverse_iterator;

  // TODO(keinakashima): add security check
  struct AddResult final {
    STACK_ALLOCATED();

   public:
    AddResult(const Value* stored_value, bool is_new_entry)
        : stored_value(stored_value), is_new_entry(is_new_entry) {}
    const Value* stored_value;
    bool is_new_entry;
  };

  typedef typename HashTraits<Value>::PeekInType ValuePeekInType;

  NewLinkedHashSet();
  NewLinkedHashSet(const NewLinkedHashSet&) = default;
  NewLinkedHashSet(NewLinkedHashSet&&) = default;
  NewLinkedHashSet& operator=(const NewLinkedHashSet&) = default;
  NewLinkedHashSet& operator=(NewLinkedHashSet&&) = default;

  ~NewLinkedHashSet() = default;

  void Swap(NewLinkedHashSet&);

  wtf_size_t size() const { return list_.size(); }
  bool IsEmpty() const { return list_.empty(); }

  iterator begin() { return list_.begin(); }
  const_iterator begin() const { return list_.cbegin(); }
  const_iterator cbegin() const { return list_.cbegin(); }
  iterator end() { return list_.end(); }
  const_iterator end() const { return list_.cend(); }
  const_iterator cend() const { return list_.cend(); }

  reverse_iterator rbegin() { return list_.rbegin(); }
  const_reverse_iterator rbegin() const { return list_.crbegin(); }
  const_reverse_iterator crbegin() const { return list_.crbegin(); }
  reverse_iterator rend() { return list_.rend(); }
  const_reverse_iterator rend() const { return list_.crend(); }
  const_reverse_iterator crend() const { return list_.crend(); }

  const Value& front() const { return list_.front(); }
  const Value& back() const { return list_.back(); }

  iterator find(ValuePeekInType);
  const_iterator find(ValuePeekInType) const;
  bool Contains(ValuePeekInType) const;

  template <typename IncomingValueType>
  AddResult insert(IncomingValueType&&);

  // If |value| already exists in the set, nothing happens.
  // If |before_value| doesn't exist in the set, appends |value|.
  template <typename IncomingValueType>
  AddResult InsertBefore(ValuePeekInType before_value,
                         IncomingValueType&& value);

  template <typename IncomingValueType>
  AddResult InsertBefore(const_iterator it, IncomingValueType&& value);

  template <typename IncomingValueType>
  AddResult AppendOrMoveToLast(IncomingValueType&&);

  template <typename IncomingValueType>
  AddResult PrependOrMoveToFirst(IncomingValueType&&);

  void erase(ValuePeekInType);
  void erase(const_iterator);
  void RemoveFirst();
  void pop_back();

  void clear() {
    value_to_index_.clear();
    list_.clear();
  }

  template <typename VisitorDispatcher, typename A = Allocator>
  std::enable_if_t<A::kIsGarbageCollected> Trace(VisitorDispatcher visitor) {
    value_to_index_.Trace(visitor);
    list_.Trace(visitor);
  }

 private:
  enum class MoveType {
    kMoveIfValueExists,
    kDontMove,
  };

  template <typename IncomingValueType>
  AddResult InsertOrMoveBefore(const_iterator, IncomingValueType&&, MoveType);

  Map value_to_index_;
  ListType list_;
};

template <typename T, typename Allocator>
inline NewLinkedHashSet<T, Allocator>::NewLinkedHashSet() {
  static_assert(Allocator::kIsGarbageCollected ||
                    !IsPointerToGarbageCollectedType<T>::value,
                "Cannot put raw pointers to garbage-collected classes into "
                "an off-heap NewLinkedHashSet. Use "
                "HeapNewLinkedHashSet<Member<T>> instead.");
}

template <typename T, typename Allocator>
inline void NewLinkedHashSet<T, Allocator>::Swap(NewLinkedHashSet& other) {
  value_to_index_.swap(other.value_to_index_);
  list_.swap(other.list_);
}

template <typename T, typename Allocator>
typename NewLinkedHashSet<T, Allocator>::iterator
NewLinkedHashSet<T, Allocator>::find(ValuePeekInType value) {
  typename Map::const_iterator it = value_to_index_.find(value);

  if (it == value_to_index_.end())
    return end();
  return list_.MakeIterator(it->value);
}

template <typename T, typename Allocator>
typename NewLinkedHashSet<T, Allocator>::const_iterator
NewLinkedHashSet<T, Allocator>::find(ValuePeekInType value) const {
  typename Map::const_iterator it = value_to_index_.find(value);

  if (it == value_to_index_.end())
    return end();
  return list_.MakeConstIterator(it->value);
}

template <typename T, typename Allocator>
bool NewLinkedHashSet<T, Allocator>::Contains(ValuePeekInType value) const {
  return value_to_index_.Contains(value);
}

template <typename T, typename Allocator>
template <typename IncomingValueType>
typename NewLinkedHashSet<T, Allocator>::AddResult
NewLinkedHashSet<T, Allocator>::insert(IncomingValueType&& value) {
  return InsertOrMoveBefore(end(), std::forward<IncomingValueType>(value),
                            MoveType::kDontMove);
}

template <typename T, typename Allocator>
template <typename IncomingValueType>
typename NewLinkedHashSet<T, Allocator>::AddResult
NewLinkedHashSet<T, Allocator>::InsertBefore(ValuePeekInType before_value,
                                             IncomingValueType&& value) {
  return InsertOrMoveBefore(find(before_value),
                            std::forward<IncomingValueType>(value),
                            MoveType::kDontMove);
}

template <typename T, typename Allocator>
template <typename IncomingValueType>
typename NewLinkedHashSet<T, Allocator>::AddResult
NewLinkedHashSet<T, Allocator>::InsertBefore(const_iterator it,
                                             IncomingValueType&& value) {
  return InsertOrMoveBefore(it, std::forward<IncomingValueType>(value),
                            MoveType::kDontMove);
}

template <typename T, typename Allocator>
template <typename IncomingValueType>
typename NewLinkedHashSet<T, Allocator>::AddResult
NewLinkedHashSet<T, Allocator>::AppendOrMoveToLast(IncomingValueType&& value) {
  return InsertOrMoveBefore(end(), std::forward<IncomingValueType>(value),
                            MoveType::kMoveIfValueExists);
}

template <typename T, typename Allocator>
template <typename IncomingValueType>
typename NewLinkedHashSet<T, Allocator>::AddResult
NewLinkedHashSet<T, Allocator>::PrependOrMoveToFirst(
    IncomingValueType&& value) {
  return InsertOrMoveBefore(begin(), std::forward<IncomingValueType>(value),
                            MoveType::kMoveIfValueExists);
}

template <typename T, typename Allocator>
inline void NewLinkedHashSet<T, Allocator>::erase(ValuePeekInType value) {
  erase(find(value));
}

template <typename T, typename Allocator>
inline void NewLinkedHashSet<T, Allocator>::erase(const_iterator it) {
  if (it == end())
    return;
  value_to_index_.erase(*it);
  list_.erase(it);
}

template <typename T, typename Allocator>
inline void NewLinkedHashSet<T, Allocator>::RemoveFirst() {
  DCHECK(!IsEmpty());
  erase(begin());
}

template <typename T, typename Allocator>
inline void NewLinkedHashSet<T, Allocator>::pop_back() {
  DCHECK(!IsEmpty());
  erase(--end());
}

template <typename T, typename Allocator>
template <typename IncomingValueType>
typename NewLinkedHashSet<T, Allocator>::AddResult
NewLinkedHashSet<T, Allocator>::InsertOrMoveBefore(const_iterator position,
                                                   IncomingValueType&& value,
                                                   MoveType type) {
  typename Map::AddResult result = value_to_index_.insert(value, kNotFound);

  if (result.is_new_entry) {
    const_iterator stored_position_iterator =
        list_.insert(position, std::forward<IncomingValueType>(value));
    result.stored_value->value = stored_position_iterator.GetIndex();
    return AddResult(stored_position_iterator.Get(), true);
  }

  const_iterator stored_position_iterator =
      list_.MakeConstIterator(result.stored_value->value);
  if (type == MoveType::kDontMove)
    return AddResult(stored_position_iterator.Get(), false);

  const_iterator moved_position_iterator =
      list_.MoveTo(stored_position_iterator, position);
  return AddResult(moved_position_iterator.Get(), false);
}

}  // namespace WTF

using WTF::LinkedHashSet;
using WTF::NewLinkedHashSet;

#endif /* WTF_LinkedHashSet_h */
