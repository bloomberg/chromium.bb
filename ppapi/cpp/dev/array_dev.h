// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_ARRAY_DEV_H_
#define PPAPI_CPP_DEV_ARRAY_DEV_H_

#include <cstdlib>

#include <vector>

#include "ppapi/cpp/dev/may_own_ptr_dev.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/output_traits.h"

namespace pp {

template <typename T>
class Array;

namespace internal {

class ArrayAllocator {
 public:
  // Allocates memory and zero-fills it.
  static void* Alloc(uint32_t count, uint32_t size) {
    if (count == 0 || size == 0)
      return NULL;

    return calloc(count, size);
  }

  static void Free(void* mem) { free(mem); }

  static PP_ArrayOutput Get() {
    PP_ArrayOutput array_output = { &ArrayAllocator::GetDataBuffer, NULL };
    return array_output;
  }

 private:
  static void* GetDataBuffer(void* /* user_data */,
                             uint32_t element_count,
                             uint32_t element_size) {
    return Alloc(element_count, element_size);
  }
};

// A specialization of CallbackOutputTraits for pp::Array parameters.
template <typename T>
struct CallbackOutputTraits<Array<T> > {
  typedef typename Array<T>::CArrayType* APIArgType;
  typedef Array<T> StorageType;

  // Returns the underlying C struct of |t|, which can be passed to the browser
  // as an output parameter.
  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return t.StartRawUpdate();
  }

  // For each |t| passed into StorageToAPIArg(), this method must be called
  // exactly once in order to match StartRawUpdate() and EndRawUpdate().
  static inline Array<T>& StorageToPluginArg(StorageType& t) {
    t.EndRawUpdate();
    return t;
  }

  static inline void Initialize(StorageType* /* t */) {}
};

}  // namespace internal

// Generic array for struct wrappers.
template <class T>
class Array {
 public:
  typedef typename T::CArrayType CArrayType;
  typedef typename T::CType CType;

  Array() {}

  explicit Array(uint32_t size) { Reset(size); }

  // Creates an accessor to |storage| but doesn't take ownership of the memory.
  // |storage| must live longer than this object.
  //
  // Although this object doesn't own the memory of |storage|, it manages the
  // memory pointed to by |storage->elements| and resets |storage| to empty when
  // destructed.
  Array(CArrayType* storage, NotOwned) : storage_(storage, NOT_OWNED) {
    CreateWrappers();
  }

  Array(const Array<T>& other) { DeepCopy(*other.storage_); }

  ~Array() { Reset(0); }

  Array<T>& operator=(const Array<T>& other) {
    return operator=(*other.storage_);
  }

  Array<T>& operator=(const CArrayType& other) {
    if (storage_.get() == &other)
      return *this;

    Reset(0);
    DeepCopy(other);

    return *this;
  }

  uint32_t size() const { return storage_->size; }

  T& operator[](uint32_t index) {
    PP_DCHECK(storage_->size == wrappers_.size());
    PP_DCHECK(index < storage_->size);
    PP_DCHECK(wrappers_[index]->ToStruct() == storage_->elements + index);
    return *wrappers_[index];
  }

  const T& operator[](uint32_t index) const {
    PP_DCHECK(storage_->size == wrappers_.size());
    PP_DCHECK(index < storage_->size);
    PP_DCHECK(wrappers_[index]->ToStruct() == storage_->elements + index);
    return *wrappers_[index];
  }

  // Clears the existing contents of the array, and resets it to |size| elements
  // of default value.
  void Reset(uint32_t size) {
    // Wrappers must be destroyed before we free |storage_->elements|, because
    // they may try to access the memory in their destructors.
    DeleteWrappers();

    internal::ArrayAllocator::Free(storage_->elements);
    storage_->elements = NULL;

    storage_->size = size;
    if (size > 0) {
      storage_->elements = static_cast<CType*>(
          internal::ArrayAllocator::Alloc(size, sizeof(CType)));
    }

    CreateWrappers();
  }

  // Sets the underlying C array struct to empty before returning it.
  CArrayType* StartRawUpdate() {
    Reset(0);
    return storage_.get();
  }

  void EndRawUpdate() { CreateWrappers(); }

 private:
  void DeepCopy(const CArrayType& other) {
    storage_->size = other.size;

    if (storage_->size > 0) {
      storage_->elements = static_cast<CType*>(
          internal::ArrayAllocator::Alloc(storage_->size, sizeof(CType)));
    }

    CreateWrappers();

    for (size_t i = 0; i < storage_->size; ++i)
      wrappers_[i] = other.elements[i];
  }

  void DeleteWrappers() {
    for (typename std::vector<T*>::iterator iter = wrappers_.begin();
         iter != wrappers_.end();
         ++iter) {
      delete *iter;
    }
    wrappers_.clear();
  }

  void CreateWrappers() {
    PP_DCHECK(wrappers_.empty());

    uint32_t size = storage_->size;
    if (size == 0)
      return;

    wrappers_.reserve(size);
    for (size_t i = 0; i < size; ++i)
      wrappers_.push_back(new T(&storage_->elements[i], NOT_OWNED));
  }

  internal::MayOwnPtr<CArrayType> storage_;
  std::vector<T*> wrappers_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_ARRAY_DEV_H_
