// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_PROPERTY_METADATA_H_
#define UI_VIEWS_METADATA_PROPERTY_METADATA_H_

#include <string>
#include <type_traits>
#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/metadata/metadata_cache.h"
#include "ui/views/metadata/metadata_types.h"
#include "ui/views/metadata/type_conversion.h"
#include "ui/views/views_export.h"

namespace views {
namespace metadata {

// Represents meta data for a specific read-only property member of class
// |TClass|, with underlying type |TValue|, as the type of the actual member.
// Using a separate |TRet| type for the getter function's return type to allow
// it to return a type with qualifier and by reference.
template <typename TClass,
          typename TValue,
          typename TRet,
          TRet (TClass::*Get)() const>
class ClassPropertyReadOnlyMetaData : public MemberMetaDataBase {
 public:
  using MemberMetaDataBase::MemberMetaDataBase;
  ~ClassPropertyReadOnlyMetaData() override = default;

  base::string16 GetValueAsString(void* obj) const override {
    if (!kIsSerializable)
      return base::string16();
    return TypeConverter<TValue>::ToString((static_cast<TClass*>(obj)->*Get)());
  }

  PropertyFlags GetPropertyFlags() const override {
    return kIsSerializable
               ? (PropertyFlags::kReadOnly | PropertyFlags::kSerializable)
               : PropertyFlags::kReadOnly;
  }

 private:
  static constexpr bool kIsSerializable =
      TypeConverter<TValue>::is_serializable;

  DISALLOW_COPY_AND_ASSIGN(ClassPropertyReadOnlyMetaData);
};

// Represents meta data for a specific property member of class |TClass|, with
// underlying type |TValue|, as the type of the actual member.
// Allows for interaction with the property as if it were the underlying data
// type (|TValue|), but still uses the Property's functionality under the hood
// (so it will trigger things like property changed notifications).
template <typename TClass,
          typename TValue,
          typename TSig,
          TSig Set,
          typename TRet,
          TRet (TClass::*Get)() const>
class ClassPropertyMetaData
    : public ClassPropertyReadOnlyMetaData<TClass, TValue, TRet, Get> {
 public:
  using ClassPropertyReadOnlyMetaData<TClass, TValue, TRet, Get>::
      ClassPropertyReadOnlyMetaData;
  ~ClassPropertyMetaData() override = default;

  void SetValueAsString(void* obj, const base::string16& new_value) override {
    if (!kIsSerializable)
      return;
    if (base::Optional<TValue> result =
            TypeConverter<TValue>::FromString(new_value)) {
      (static_cast<TClass*>(obj)->*Set)(std::move(result.value()));
    }
  }

  PropertyFlags GetPropertyFlags() const override {
    return kIsSerializable
               ? (PropertyFlags::kEmpty | PropertyFlags::kSerializable)
               : PropertyFlags::kEmpty;
  }

 private:
  static constexpr bool kIsSerializable =
      TypeConverter<TValue>::is_serializable;

  DISALLOW_COPY_AND_ASSIGN(ClassPropertyMetaData);
};

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_PROPERTY_METADATA_H_
