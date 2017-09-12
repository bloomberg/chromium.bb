/*
 *  Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc.
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

// PassRefPtr will soon be deleted.
// New code should instead pass ownership of the contents of a RefPtr using
// std::move().

#ifndef WTF_PassRefPtr_h
#define WTF_PassRefPtr_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Compiler.h"
#include "platform/wtf/TypeTraits.h"

namespace WTF {

template <typename T>
class RefPtr;
template <typename T>
class PassRefPtr;

// requireAdoption() is not overloaded for WTF::RefCounted, which has a built-in
// assumption that adoption is required. requireAdoption() is for bootstrapping
// alternate reference count classes that are compatible with RefPtr/PassRefPtr
// but cannot have adoption checks enabled by default, such as skia's
// SkRefCnt. The purpose of requireAdoption() is to enable adoption checks only
// once it is known that the object will be used with RefPtr/PassRefPtr.
inline void RequireAdoption(const void*) {}

template <typename T>
ALWAYS_INLINE void RefIfNotNull(T* ptr) {
  if (LIKELY(ptr != 0)) {
    RequireAdoption(ptr);
    ptr->Ref();
  }
}

template <typename T>
ALWAYS_INLINE void DerefIfNotNull(T* ptr) {
  if (LIKELY(ptr != 0))
    ptr->Deref();
}

template <typename T>
class PassRefPtr {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  PassRefPtr() : ptr_(nullptr) {}
  PassRefPtr(std::nullptr_t) : ptr_(nullptr) {}
  PassRefPtr(T* ptr) : ptr_(ptr) { RefIfNotNull(ptr); }
  PassRefPtr(PassRefPtr&& o) : ptr_(o.LeakRef()) {}
  template <typename U>
  PassRefPtr(PassRefPtr<U>&& o, EnsurePtrConvertibleArgDecl(U, T))
      : ptr_(o.LeakRef()) {}

  ALWAYS_INLINE ~PassRefPtr() { DerefIfNotNull(ptr_); }

  template <typename U>
  PassRefPtr(const RefPtr<U>&, EnsurePtrConvertibleArgDecl(U, T));
  template <typename U>
  PassRefPtr(RefPtr<U>&&, EnsurePtrConvertibleArgDecl(U, T));

  T* Get() const { return ptr_; }

  WARN_UNUSED_RESULT T* LeakRef();

  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  bool operator!() const { return !ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  PassRefPtr& operator=(const PassRefPtr&) {
    static_assert(!sizeof(T*), "PassRefPtr should never be assigned to");
    return *this;
  }

  T* ptr_;
};

template <typename T>
PassRefPtr<T> WrapPassRefPtr(T* ptr) {
  return PassRefPtr<T>(ptr);
}

template <typename T>
template <typename U>
inline PassRefPtr<T>::PassRefPtr(const RefPtr<U>& o,
                                 EnsurePtrConvertibleArgDefn(U, T))
    : ptr_(o.Get()) {
  T* ptr = ptr_;
  RefIfNotNull(ptr);
}

template <typename T>
template <typename U>
inline PassRefPtr<T>::PassRefPtr(RefPtr<U>&& o,
                                 EnsurePtrConvertibleArgDefn(U, T))
    : ptr_(o.LeakRef()) {}

template <typename T>
inline T* PassRefPtr<T>::LeakRef() {
  T* ptr = ptr_;
  ptr_ = nullptr;
  return ptr;
}

template <typename T, typename U>
inline bool operator==(const PassRefPtr<T>& a, const PassRefPtr<U>& b) {
  return a.Get() == b.Get();
}

template <typename T, typename U>
inline bool operator==(const PassRefPtr<T>& a, const RefPtr<U>& b) {
  return a.Get() == b.Get();
}

template <typename T, typename U>
inline bool operator==(const RefPtr<T>& a, const PassRefPtr<U>& b) {
  return a.Get() == b.Get();
}

template <typename T, typename U>
inline bool operator==(const PassRefPtr<T>& a, U* b) {
  return a.Get() == b;
}

template <typename T, typename U>
inline bool operator==(T* a, const PassRefPtr<U>& b) {
  return a == b.Get();
}

template <typename T>
inline bool operator==(const PassRefPtr<T>& a, std::nullptr_t) {
  return !a.Get();
}

template <typename T>
inline bool operator==(std::nullptr_t, const PassRefPtr<T>& b) {
  return !b.Get();
}

template <typename T, typename U>
inline bool operator!=(const PassRefPtr<T>& a, const PassRefPtr<U>& b) {
  return a.Get() != b.Get();
}

template <typename T, typename U>
inline bool operator!=(const PassRefPtr<T>& a, const RefPtr<U>& b) {
  return a.Get() != b.Get();
}

template <typename T, typename U>
inline bool operator!=(const RefPtr<T>& a, const PassRefPtr<U>& b) {
  return a.Get() != b.Get();
}

template <typename T, typename U>
inline bool operator!=(const PassRefPtr<T>& a, U* b) {
  return a.Get() != b;
}

template <typename T, typename U>
inline bool operator!=(T* a, const PassRefPtr<U>& b) {
  return a != b.Get();
}

template <typename T>
inline bool operator!=(const PassRefPtr<T>& a, std::nullptr_t) {
  return a.Get();
}

template <typename T>
inline bool operator!=(std::nullptr_t, const PassRefPtr<T>& b) {
  return b.Get();
}

template <typename T>
inline T* GetPtr(const PassRefPtr<T>& p) {
  return p.Get();
}

}  // namespace WTF

using WTF::PassRefPtr;

#endif  // WTF_PassRefPtr_h
