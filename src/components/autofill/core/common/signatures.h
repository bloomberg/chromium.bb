// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_SIGNATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_SIGNATURES_H_

#include <stddef.h>

#include <stdint.h>
#include <string>

#include "base/strings/string16.h"
#include "base/util/type_safety/id_type.h"

namespace autofill {

struct FormData;
struct FormFieldData;

namespace internal {

using FormSignatureType =
    ::util::IdType<class FormSignatureMarker, uint64_t, 0>;

using FieldSignatureType =
    ::util::IdType<class FieldSignatureMarker, uint32_t, 0>;

}  // namespace internal

// The below strong aliases are defined as subclasses instead of typedefs in
// order to avoid having to define out-of-line constructors in all structs that
// contain signatures.

class FormSignature : public internal::FormSignatureType {
  using internal::FormSignatureType::IdType;
};

class FieldSignature : public internal::FieldSignatureType {
  using internal::FieldSignatureType::IdType;
};

// Calculates form signature based on |form_data|.
FormSignature CalculateFormSignature(const FormData& form_data);

// Calculates field signature based on |field_name| and |field_type|.
FieldSignature CalculateFieldSignatureByNameAndType(
    const base::string16& field_name,
    const std::string& field_type);

// Calculates field signature based on |field_data|. This function is a proxy to
// |CalculateFieldSignatureByNameAndType|.
FieldSignature CalculateFieldSignatureForField(const FormFieldData& field_data);

// Returns 64-bit hash of the string.
uint64_t StrToHash64Bit(const std::string& str);

// Returns 32-bit hash of the string.
uint32_t StrToHash32Bit(const std::string& str);

// Reduce FieldSignature space (in UKM) to a small range for privacy reasons.
int64_t HashFormSignature(autofill::FormSignature form_signature);

// Reduce FieldSignature space (in UKM) to a small range for privacy reasons.
int64_t HashFieldSignature(autofill::FieldSignature field_signature);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_SIGNATURES_H_
