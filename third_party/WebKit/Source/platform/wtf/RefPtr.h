/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc.
 *  All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_RefPtr_h
#define WTF_RefPtr_h

#include <algorithm>
#include <utility>
#include "base/memory/ref_counted.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/allocator/PartitionAllocator.h"

namespace WTF {

template <typename T>
class RefPtrValuePeeker;
template <typename T>
class RefPtr;

template <typename T>
RefPtr<T> AdoptRef(T*);

template <typename T>
class RefPtr {
  USING_FAST_MALLOC(RefPtr);

 public:
  ALWAYS_INLINE RefPtr() {}
  ALWAYS_INLINE RefPtr(std::nullptr_t) {}
  ALWAYS_INLINE RefPtr(T* ptr) : ptr_(ptr) {}
  ALWAYS_INLINE RefPtr(const RefPtr&) = default;
  ALWAYS_INLINE RefPtr(RefPtr&&) = default;

  template <typename U>
  RefPtr(const RefPtr<U>& o, EnsurePtrConvertibleArgDecl(U, T))
      : ptr_(o.ptr_) {}
  template <typename U>
  RefPtr(RefPtr<U>&& o, EnsurePtrConvertibleArgDecl(U, T))
      : ptr_(std::move(o.ptr_)) {}

  ALWAYS_INLINE ~RefPtr() {}

  ALWAYS_INLINE T* get() const { return ptr_.get(); }

  T& operator*() const { return *ptr_; }
  ALWAYS_INLINE T* operator->() const { return ptr_.get(); }

  bool operator!() const { return !ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

  RefPtr& operator=(const RefPtr&) = default;
  RefPtr& operator=(RefPtr&&) = default;

  RefPtr& operator=(std::nullptr_t) {
    ptr_ = nullptr;
    return *this;
  }
  // This is required by HashMap<RefPtr>>.
  template <typename U>
  RefPtr& operator=(RefPtrValuePeeker<U>);

  void swap(RefPtr&);

 private:
  template <typename U>
  friend class RefPtr;
  friend RefPtr AdoptRef<T>(T*);

  ALWAYS_INLINE explicit RefPtr(scoped_refptr<T> ptr) : ptr_(std::move(ptr)) {}

  scoped_refptr<T> ptr_;
};

template <typename T>
template <typename U>
inline RefPtr<T>& RefPtr<T>::operator=(RefPtrValuePeeker<U> optr) {
  RefPtr ptr = static_cast<U*>(optr);
  swap(ptr);
  return *this;
}

template <class T>
inline void RefPtr<T>::swap(RefPtr& o) {
  std::swap(ptr_, o.ptr_);
}

template <class T>
inline void swap(RefPtr<T>& a, RefPtr<T>& b) {
  a.swap(b);
}

template <typename T, typename U>
inline bool operator==(const RefPtr<T>& a, const RefPtr<U>& b) {
  return a.get() == b.get();
}

template <typename T, typename U>
inline bool operator==(const RefPtr<T>& a, U* b) {
  return a.get() == b;
}

template <typename T, typename U>
inline bool operator==(T* a, const RefPtr<U>& b) {
  return a == b.get();
}

template <typename T>
inline bool operator==(const RefPtr<T>& a, std::nullptr_t) {
  return !a.get();
}

template <typename T>
inline bool operator==(std::nullptr_t, const RefPtr<T>& b) {
  return !b.get();
}

template <typename T, typename U>
inline bool operator!=(const RefPtr<T>& a, const RefPtr<U>& b) {
  return a.get() != b.get();
}

template <typename T, typename U>
inline bool operator!=(const RefPtr<T>& a, U* b) {
  return a.get() != b;
}

template <typename T, typename U>
inline bool operator!=(T* a, const RefPtr<U>& b) {
  return a != b.get();
}

template <typename T>
inline bool operator!=(const RefPtr<T>& a, std::nullptr_t) {
  return a.get();
}

template <typename T>
inline bool operator!=(std::nullptr_t, const RefPtr<T>& b) {
  return b.get();
}

template <typename T>
inline T* GetPtr(const RefPtr<T>& p) {
  return p.get();
}

template <typename T>
class RefPtrValuePeeker {
  DISALLOW_NEW();

 public:
  ALWAYS_INLINE RefPtrValuePeeker(T* p) : ptr_(p) {}
  ALWAYS_INLINE RefPtrValuePeeker(std::nullptr_t) : ptr_(nullptr) {}
  template <typename U>
  RefPtrValuePeeker(const RefPtr<U>& p) : ptr_(p.get()) {}

  ALWAYS_INLINE operator T*() const { return ptr_; }

 private:
  T* ptr_;
};

template <typename T>
RefPtr<T> AdoptRef(T* p) {
  return RefPtr<T>(base::AdoptRef(p));
}

template <typename T>
RefPtr<T> WrapRefPtr(T* ptr) {
  return RefPtr<T>(ptr);
}

}  // namespace WTF

using WTF::RefPtr;

#endif  // WTF_RefPtr_h
