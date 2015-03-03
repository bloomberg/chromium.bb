// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace mojo {
namespace internal {

// Data types for keys.
template <typename MapKey>
struct MapKeyValidateParams {
 public:
  typedef NoValidateParams ElementValidateParams;
  static const uint32_t expected_num_elements = 0;
  static const bool element_is_nullable = false;
};

// For non-nullable strings only. (Which is OK; map keys can't be null.)
template <>
struct MapKeyValidateParams<mojo::internal::Array_Data<char>*> {
 public:
  typedef ArrayValidateParams<0, false, NoValidateParams> ElementValidateParams;
  static const uint32_t expected_num_elements = 0;
  static const bool element_is_nullable = false;
};

// Map serializes into a struct which has two arrays as struct fields, the keys
// and the values.
template <typename Key, typename Value>
class Map_Data {
 public:
  static Map_Data* New(Buffer* buf) {
    return new (buf->Allocate(sizeof(Map_Data))) Map_Data();
  }

  template <typename ValueParams>
  static bool Validate(const void* data, BoundsChecker* bounds_checker) {
    if (!data)
      return true;

    if (!ValidateStructHeaderAndClaimMemory(data, bounds_checker))
      return false;

    const Map_Data* object = static_cast<const Map_Data*>(data);
    // TODO(yzshen): In order to work with other bindings which still interprets
    // the |version| field as |num_fields|, |version| is set to 2.
    if (object->header_.num_bytes != sizeof(Map_Data) ||
        object->header_.version != 2) {
      ReportValidationError(VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
      return false;
    }

    if (!ValidateEncodedPointer(&object->keys.offset)) {
      ReportValidationError(VALIDATION_ERROR_ILLEGAL_POINTER);
      return false;
    }
    if (!object->keys.offset) {
      ReportValidationError(VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                            "null key array in map struct");
      return false;
    }
    if (!Array_Data<Key>::template Validate<MapKeyValidateParams<Key>>(
            DecodePointerRaw(&object->keys.offset), bounds_checker)) {
      return false;
    }

    if (!ValidateEncodedPointer(&object->values.offset)) {
      ReportValidationError(VALIDATION_ERROR_ILLEGAL_POINTER);
      return false;
    }
    if (!object->values.offset) {
      ReportValidationError(VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
                            "null value array in map struct");
      return false;
    }
    if (!Array_Data<Value>::template Validate<ValueParams>(
            DecodePointerRaw(&object->values.offset), bounds_checker)) {
      return false;
    }

    const ArrayHeader* key_header =
        static_cast<const ArrayHeader*>(DecodePointerRaw(&object->keys.offset));
    const ArrayHeader* value_header = static_cast<const ArrayHeader*>(
        DecodePointerRaw(&object->values.offset));
    if (key_header->num_elements != value_header->num_elements) {
      ReportValidationError(VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP);
      return false;
    }

    return true;
  }

  StructHeader header_;

  ArrayPointer<Key> keys;
  ArrayPointer<Value> values;

  void EncodePointersAndHandles(std::vector<mojo::Handle>* handles) {
    Encode(&keys, handles);
    Encode(&values, handles);
  }

  void DecodePointersAndHandles(std::vector<mojo::Handle>* handles) {
    Decode(&keys, handles);
    Decode(&values, handles);
  }

 private:
  Map_Data() {
    header_.num_bytes = sizeof(*this);
    // TODO(yzshen): In order to work with other bindings which still interprets
    // the |version| field as |num_fields|, set it to version 2 for now.
    header_.version = 2;
  }
  ~Map_Data() = delete;
};
static_assert(sizeof(Map_Data<char, char>) == 24, "Bad sizeof(Map_Data)");

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_MAP_DATA_INTERNAL_H_
