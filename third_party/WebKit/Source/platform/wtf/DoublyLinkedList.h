/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DoublyLinkedList_h
#define DoublyLinkedList_h

#include "platform/wtf/Allocator.h"

namespace WTF {

// This class allows nodes to share code without dictating data member layout.
template <typename T>
class DoublyLinkedListNode {
 public:
  DoublyLinkedListNode();

  void SetPrev(T*);
  void SetNext(T*);

  T* Prev() const;
  T* Next() const;
};

template <typename T>
inline DoublyLinkedListNode<T>::DoublyLinkedListNode() {
  SetPrev(0);
  SetNext(0);
}

template <typename T>
inline void DoublyLinkedListNode<T>::SetPrev(T* prev) {
  static_cast<T*>(this)->prev_ = prev;
}

template <typename T>
inline void DoublyLinkedListNode<T>::SetNext(T* next) {
  static_cast<T*>(this)->next_ = next;
}

template <typename T>
inline T* DoublyLinkedListNode<T>::Prev() const {
  return static_cast<const T*>(this)->prev_;
}

template <typename T>
inline T* DoublyLinkedListNode<T>::Next() const {
  return static_cast<const T*>(this)->next_;
}

template <typename T>
class DoublyLinkedList {
  USING_FAST_MALLOC(DoublyLinkedList);

 public:
  DoublyLinkedList();

  bool IsEmpty() const;
  size_t size() const;  // This is O(n).
  void Clear();

  T* Head() const;
  T* RemoveHead();

  T* Tail() const;

  void Push(T*);
  void Append(T*);
  void Remove(T*);

 private:
  T* head_;
  T* tail_;
};

template <typename T>
inline DoublyLinkedList<T>::DoublyLinkedList() : head_(0), tail_(0) {}

template <typename T>
inline bool DoublyLinkedList<T>::IsEmpty() const {
  return !head_;
}

template <typename T>
inline size_t DoublyLinkedList<T>::size() const {
  size_t size = 0;
  for (T* node = head_; node; node = node->Next())
    ++size;
  return size;
}

template <typename T>
inline void DoublyLinkedList<T>::Clear() {
  head_ = 0;
  tail_ = 0;
}

template <typename T>
inline T* DoublyLinkedList<T>::Head() const {
  return head_;
}

template <typename T>
inline T* DoublyLinkedList<T>::Tail() const {
  return tail_;
}

template <typename T>
inline void DoublyLinkedList<T>::Push(T* node) {
  if (!head_) {
    DCHECK(!tail_);
    head_ = node;
    tail_ = node;
    node->SetPrev(0);
    node->SetNext(0);
    return;
  }

  DCHECK(tail_);
  head_->SetPrev(node);
  node->SetNext(head_);
  node->SetPrev(0);
  head_ = node;
}

template <typename T>
inline void DoublyLinkedList<T>::Append(T* node) {
  if (!tail_) {
    DCHECK(!head_);
    head_ = node;
    tail_ = node;
    node->SetPrev(0);
    node->SetNext(0);
    return;
  }

  DCHECK(head_);
  tail_->SetNext(node);
  node->SetPrev(tail_);
  node->SetNext(0);
  tail_ = node;
}

template <typename T>
inline void DoublyLinkedList<T>::Remove(T* node) {
  if (node->Prev()) {
    DCHECK_NE(node, head_);
    node->Prev()->SetNext(node->Next());
  } else {
    DCHECK_EQ(node, head_);
    head_ = node->Next();
  }

  if (node->Next()) {
    DCHECK_NE(node, tail_);
    node->Next()->SetPrev(node->Prev());
  } else {
    DCHECK_EQ(node, tail_);
    tail_ = node->Prev();
  }
}

template <typename T>
inline T* DoublyLinkedList<T>::RemoveHead() {
  T* node = Head();
  if (node)
    Remove(node);
  return node;
}

}  // namespace WTF

using WTF::DoublyLinkedListNode;
using WTF::DoublyLinkedList;

#endif
