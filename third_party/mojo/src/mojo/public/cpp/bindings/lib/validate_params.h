// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_

namespace mojo {
namespace internal {

// Array type information needed for valdiation.
template <uint32_t in_expected_num_elements,
          bool in_element_is_nullable,
          typename InElementValidateParams>
class ArrayValidateParams {
 public:
  // Validation information for elements. It is either another specialization
  // of ArrayValidateParams (if elements are arrays or maps), or
  // NoValidateParams. In the case of maps, this is used to validate the value
  // array.
  typedef InElementValidateParams ElementValidateParams;

  // If |expected_num_elements| is not 0, the array is expected to have exactly
  // that number of elements.
  static const uint32_t expected_num_elements = in_expected_num_elements;
  // Whether the elements are nullable.
  static const bool element_is_nullable = in_element_is_nullable;
};

// NoValidateParams is used to indicate the end of an ArrayValidateParams chain.
class NoValidateParams {};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_VALIDATE_PARAMS_H_
