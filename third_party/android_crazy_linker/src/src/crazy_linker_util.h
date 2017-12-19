// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_UTIL_H
#define CRAZY_LINKER_UTIL_H

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

namespace crazy {

// Helper macro to loop around EINTR errors in syscalls.
#define HANDLE_EINTR(expr) TEMP_FAILURE_RETRY(expr)

// Helper macro to tag unused variables. Use in the declaration, between
// the type and name, as in:
//     int CRAZY_UNUSED my_var = 0;
#define CRAZY_UNUSED __attribute__((unused))

// Offset in a file indicating a failure.
#define CRAZY_OFFSET_FAILED (-1)

// Helper scoped pointer class.
template <class T>
class ScopedPtr {
 public:
  ScopedPtr() : ptr_(NULL) {}
  explicit ScopedPtr(T* ptr) : ptr_(ptr) {}
  ~ScopedPtr() { Reset(NULL); }

  T* Release() {
    T* ret = ptr_;
    ptr_ = NULL;
    return ret;
  }

  void Reset(T* ptr) {
    if (ptr_)
      delete ptr_;
    ptr_ = ptr;
  }

  T* Get() { return ptr_; }
  T& operator*() { return *ptr_; }
  T* operator->() { return ptr_; }

 private:
  T* ptr_;
};

// Return the base name from a file path. Important: this is a pointer
// into the original string.
const char* GetBaseNamePtr(const char* path);

// Helper class used to implement a string. Similar to std::string
// without all the crazy iterator / iostream stuff.
//
// Required because crazy linker should only link against the system
// libstdc++ that only provides new/delete.
//
class String {
 public:
  String();
  String(const char* str, size_t len);
  String(const String& other);
  explicit String(const char* str);
  explicit String(char ch);

  ~String();

  const char* c_str() const { return ptr_; }
  char* ptr() { return ptr_; }
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }

  bool IsEmpty() const { return size_ == 0; }

  char& operator[](size_t index) { return ptr_[index]; }

  String& operator=(const String& other) {
    Assign(other.ptr_, other.size_);
    return *this;
  }

  String& operator=(const char* str) {
    Assign(str, strlen(str));
    return *this;
  }

  String& operator=(char ch) {
    Assign(&ch, 1);
    return *this;
  }

  String& operator+=(const String& other) {
    Append(other);
    return *this;
  }

  String& operator+=(const char* str) {
    Append(str, strlen(str));
    return *this;
  }

  String& operator+=(char ch) {
    Append(&ch, 1);
    return *this;
  }

  void Resize(size_t new_size);

  void Reserve(size_t new_capacity);

  void Assign(const char* str, size_t len);

  void Assign(const String& other) { Assign(other.ptr_, other.size_); }

  void Assign(const char* str) { Assign(str, strlen(str)); }

  void Append(const char* str, size_t len);

  void Append(const String& other) { Append(other.ptr_, other.size_); }

  void Append(const char* str) { Append(str, strlen(str)); }

 private:
  void Init(void) {
    ptr_ = const_cast<char*>(kEmpty);
    size_ = 0;
    capacity_ = 0;
  }

  static const char kEmpty[];

  char* ptr_;
  size_t size_;
  size_t capacity_;
};

// Helper template used to implement a simple vector or POD-struct items.
// I.e. this uses memmove() to move items during insertion / removal.
//
// Required because crazy linker should only link against the system
// libstdc++ which only provides new/delete.
//
template <class T>
class Vector {
 public:
  Vector() : items_(0), count_(0), capacity_(0) {}
  ~Vector() { free(items_); }

  T& operator[](size_t index) { return items_[index]; }

  bool IsEmpty() const { return count_ == 0; }

  void PushBack(T item) { InsertAt(static_cast<int>(count_), item); }

  T PopFirst() {
    T result = items_[0];
    RemoveAt(0);
    return result;
  }

  T PopLast() {
    T result = items_[count_ - 1];
    Resize(count_ - 1);
    return result;
  }

  void Remove(T item) {
    int index = IndexOf(item);
    if (index >= 0)
      RemoveAt(index);
  }

  void InsertAt(int index, T item);

  void RemoveAt(int index);

  int IndexOf(T item) const;

  bool Has(T item) const { return IndexOf(item) >= 0; }

  size_t GetCount() const { return count_; }

  void Reserve(size_t new_capacity);

  void Resize(size_t new_count);

 private:
  T* items_;
  size_t count_;
  size_t capacity_;
};

template <class T>
int Vector<T>::IndexOf(T item) const {
  for (size_t n = 0; n < count_; ++n) {
    if (items_[n] == item)
      return static_cast<int>(n);
  }
  return -1;
}

template <class T>
void Vector<T>::InsertAt(int index, T item) {
  if (count_ >= capacity_)
    Reserve(capacity_ + (capacity_ >> 1) + 4);

  if (index < 0)
    index = 0;
  size_t n = static_cast<size_t>(index);
  if (n > count_)
    n = count_;
  else
    memmove(items_ + n + 1, items_ + n, (count_ - n) * sizeof(T));

  items_[n] = item;
  count_++;
}

template <class T>
void Vector<T>::RemoveAt(int index) {
  if (index < 0)
    return;

  size_t n = static_cast<size_t>(index);
  if (n >= count_)
    return;

  memmove(items_ + n, items_ + n + 1, (count_ - n - 1) * sizeof(T));
  count_--;
}

template <class T>
void Vector<T>::Reserve(size_t new_capacity) {
  items_ = reinterpret_cast<T*>(realloc(items_, new_capacity * sizeof(T)));
  capacity_ = new_capacity;
  if (count_ > capacity_)
    count_ = capacity_;
}

template <class T>
void Vector<T>::Resize(size_t new_size) {
  if (new_size > capacity_)
    Reserve(new_size);

  if (new_size > count_)
    memset(items_ + count_, 0, (new_size - count_) * sizeof(T));

  count_ = new_size;
}

// Helper template class to implement a set.
// Given that the crazy linker doesn't expect to deal with hundreds
// of libraries at the same time, implement it with a vector.
template <class T>
class Set {
 public:
  Set() : items_() {}
  ~Set() {}

  // Returns the number of items in the set.
  size_t GetCount() const { return items_.GetCount(); }

  bool IsEmpty() const { return items_.IsEmpty(); }

  // Returns true iff the set contains a given item.
  bool Has(T item) const { return items_.Has(item); }

  // Add an item to the set. Returns false iff the item was already in it.
  bool Add(T item);

  // Delete an item from the set. Returns false iff the item was not in it.
  bool Del(T item);

 private:
  Vector<T> items_;
};

template <class T>
bool Set<T>::Add(T item) {
  int idx = items_.IndexOf(item);
  if (idx >= 0)
    return false;

  items_.PushBack(item);
  return true;
}

template <class T>
bool Set<T>::Del(T item) {
  int idx = items_.IndexOf(item);
  if (idx < 0)
    return false;
  items_.RemoveAt(idx);
  return true;
}

}  // namespace crazy

#endif  // CRAZY_LINKER_UTIL_H
