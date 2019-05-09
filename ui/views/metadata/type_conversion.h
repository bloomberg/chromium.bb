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

// Various metadata methods pass types either by value or const ref depending on
// whether the types are "small" (defined as "fundamental, enum, or pointer").
// ArgType<T> gives the appropriate type to use as an argument in such cases.
template <typename T>
using ArgType = typename std::conditional<std::is_fundamental<T>::value ||
                                              std::is_enum<T>::value ||
                                              std::is_pointer<T>::value,
                                          T,
                                          const T&>::type;

// TypeConverter Class --------------------------------------------------------
template <typename TSource, typename TTarget>
class TypeConverter {
 public:
  static TTarget Convert(ArgType<TSource>) = delete;
  static TTarget Convert(ArgType<TSource>, ArgType<TTarget>) = delete;

 private:
  TypeConverter();
};

// Master Type Conversion Functions --------------------------------------------
template <typename TSource, typename TTarget>
TTarget Convert(ArgType<TSource> source_value) {
  return TypeConverter<TSource, TTarget>::Convert(source_value);
}

template <typename TSource, typename TTarget>
TTarget Convert(ArgType<TSource> source_value, ArgType<TTarget> default_value) {
  return TypeConverter<TSource, TTarget>::Convert(source_value, default_value);
}

// String Conversions ---------------------------------------------------------

template <typename TSource>
base::string16 ConvertToString(ArgType<TSource>) = delete;

template <>
VIEWS_EXPORT base::string16 ConvertToString<base::string16>(
    const base::string16& source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int8_t>(int8_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int16_t>(int16_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int32_t>(int32_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<int64_t>(int64_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint8_t>(uint8_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint16_t>(uint16_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint32_t>(uint32_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<uint64_t>(uint64_t source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<float>(float source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<double>(double source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<bool>(bool source_value);

template <>
VIEWS_EXPORT base::string16 ConvertToString<gfx::Size>(
    const gfx::Size& source_value);

template <typename TSource>
class TypeConverter<TSource, base::string16> {
 public:
  static base::string16 Convert(ArgType<TSource> source_value) {
    return ConvertToString<TSource>(source_value);
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
  static base::string16 Convert(const base::string16& source_val,
                                const base::string16& default_value) {
    return ConvertToString<base::string16>(source_val);
  }
};

template <typename TTarget>
TTarget ConvertFromString(const base::string16&, ArgType<TTarget>) = delete;

template <>
VIEWS_EXPORT int8_t
ConvertFromString<int8_t>(const base::string16& source_value,
                          int8_t default_value);

template <>
VIEWS_EXPORT int16_t
ConvertFromString<int16_t>(const base::string16& source_value,
                           int16_t default_value);

template <>
VIEWS_EXPORT int32_t
ConvertFromString<int32_t>(const base::string16& source_value,
                           int32_t default_value);

template <>
VIEWS_EXPORT int64_t
ConvertFromString<int64_t>(const base::string16& source_value,
                           int64_t default_value);

template <>
VIEWS_EXPORT uint8_t
ConvertFromString<uint8_t>(const base::string16& source_value,
                           uint8_t default_value);

template <>
VIEWS_EXPORT uint16_t
ConvertFromString<uint16_t>(const base::string16& source_value,
                            uint16_t default_value);

template <>
VIEWS_EXPORT uint32_t
ConvertFromString<uint32_t>(const base::string16& source_value,
                            uint32_t default_value);

template <>
VIEWS_EXPORT uint64_t
ConvertFromString<uint64_t>(const base::string16& source_value,
                            uint64_t default_value);

template <>
VIEWS_EXPORT double ConvertFromString<double>(
    const base::string16& source_value,
    double default_value);

template <>
VIEWS_EXPORT float ConvertFromString<float>(const base::string16& source_value,
                                            float default_value);

template <>
VIEWS_EXPORT bool ConvertFromString<bool>(const base::string16& source_value,
                                          bool default_value);

template <typename TTarget>
class TypeConverter<base::string16, TTarget> {
 public:
  static TTarget Convert(const base::string16& source_value,
                         ArgType<TTarget> default_value) {
    return ConvertFromString<TTarget>(source_value, default_value);
  }
};

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_TYPE_CONVERSION_H_
