// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implement a very basic memory allocator that make direct system calls
// instead of relying on libc.
// This allocator is not thread-safe.

#ifndef ALLOCATOR_H__
#define ALLOCATOR_H__

#include <cstddef>

namespace playground {

class SystemAllocatorHelper {
 protected:
  static void *sys_allocate(size_t size);
  static void sys_deallocate(void* p, size_t size);
};

template <class T>
class SystemAllocator : SystemAllocatorHelper {
 public:
  typedef T         value_type;
  typedef T*        pointer;
  typedef const T*  const_pointer;
  typedef T&        reference;
  typedef const T&  const_reference;
  typedef size_t    size_type;
  typedef std::ptrdiff_t difference_type;

  template <class U>
  struct rebind {
    typedef SystemAllocator<U> other;
  };

  pointer address(reference value) const {
    return &value;
  }

  const_pointer address(const_reference value) const {
    return &value;
  }

  SystemAllocator() throw() { }
  SystemAllocator(const SystemAllocator& src) throw() { }
  template <class U> SystemAllocator(const SystemAllocator<U>& src) throw() { }
  ~SystemAllocator() throw() { }

  size_type max_size() const throw() {
    return (1 << 30) / sizeof(T);
  }

  pointer allocate(size_type num, const void* = 0) {
    if (num > max_size()) {
      return NULL;
    }
    return (pointer)sys_allocate(num * sizeof(T));
  }

  void construct(pointer p, const T& value) {
    new(reinterpret_cast<void *>(p))T(value);
  }

  void destroy(pointer p) {
    p->~T();
  }

  void deallocate(pointer p, size_type num) {
    sys_deallocate(p, num * sizeof(T));
  }
};

template <class T1, class T2>
bool operator== (const SystemAllocator<T1>&, const SystemAllocator<T2>&)
    throw() {
  return true;
}
template <class T1, class T2>
bool operator!= (const SystemAllocator<T1>&, const SystemAllocator<T2>&)
    throw() {
  return false;
}

}  // namespace

#endif  // ALLOCATOR_H__
