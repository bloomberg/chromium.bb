// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_TYPE_CONVERSION_H_
#define UI_VIEWS_METADATA_TYPE_CONVERSION_H_

#include <stdint.h>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/views_export.h"

namespace views {
namespace metadata {

// TypeConverter Class --------------------------------------------------------
template <typename TSource, typename TTarget>
class TypeConverter {
 public:
  static TTarget Convert(const TSource& source_val) = delete;

 private:
  TypeConverter();
};

// Master Type Conversion Function --------------------------------------------
template <typename TSource, typename TTarget>
TTarget Convert(const TSource& source_value) {
  return TypeConverter<TSource, TTarget>::Convert(source_value);
}

// String Conversions ---------------------------------------------------------

template <typename TSource>
base::string16 ConvertToString(const TSource& source_value) = delete;

template <>
VIEWS_EXPORT base::string16 ConvertToString<base::string16>(
    const base::string16& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int8_t>(const int8_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int16_t>(
    const int16_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int32_t>(
    const int32_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int64_t>(
    const int64_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint8_t>(
    const uint8_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint16_t>(
    const uint16_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint32_t>(
    const uint32_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint64_t>(
    const uint64_t& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<float>(const float& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<double>(const double& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<bool>(const bool& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<gfx::Size>(
    const gfx::Size& source_value);

template <typename TSource>
class TypeConverter<TSource, base::string16> {
 public:
  static base::string16 Convert(const TSource& source_val) {
    return ConvertToString<TSource>(source_val);
  }
};

// A specialization used for string16 conversions. Since it simply passes
// the value, it is used for both converting to and from a string16 value.
template <>
class TypeConverter<base::string16, base::string16> {
 public:
  static base::string16 Convert(const base::string16& source_val) {
    return ConvertToString<base::string16>(source_val);
  }
};

template <typename TTarget>
TTarget ConvertFromString(const base::string16& source_value) = delete;

template <>
VIEWS_EXPORT int8_t
ConvertFromString<int8_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT int16_t
ConvertFromString<int16_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT int32_t
ConvertFromString<int32_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT int64_t
ConvertFromString<int64_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT uint8_t
ConvertFromString<uint8_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT uint16_t
ConvertFromString<uint16_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT uint32_t
ConvertFromString<uint32_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT uint64_t
ConvertFromString<uint64_t>(const base::string16& source_value);

template <>
VIEWS_EXPORT double ConvertFromString<double>(
    const base::string16& source_value);

template <>
VIEWS_EXPORT float ConvertFromString<float>(const base::string16& source_value);

template <>
VIEWS_EXPORT bool ConvertFromString<bool>(const base::string16& source_value);

template <typename TTarget>
class TypeConverter<base::string16, TTarget> {
 public:
  static TTarget Convert(const base::string16& source_value) {
    return ConvertFromString<TTarget>(source_value);
  }
};

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_TYPE_CONVERSION_H_
