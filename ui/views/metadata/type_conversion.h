// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_METADATA_TYPE_CONVERSION_H_
#define UI_VIEWS_METADATA_TYPE_CONVERSION_H_

#include <stdint.h>
#include <vector>

#include "base/no_destructor.h"
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

// Types and macros for generating enum converters ----------------------------
template <typename T>
struct EnumStrings {
  struct EnumString {
    T enum_value;
    base::string16 str_value;
  };

  EnumStrings(std::vector<EnumString> init_val) : pairs(std::move(init_val)) {}

  const std::vector<EnumString> pairs;
};

template <typename T>
static const EnumStrings<T>& GetEnumStringsInstance();

// Generate the code to define a enum type to and from base::string16
// conversions. The first argument is the type T, and the rest of the argument
// should have the enum value and string pairs defined in a format like
// "{enum_value0, string16_value0}, {enum_value1, string16_value1} ...".
#define DEFINE_ENUM_CONVERTERS(T, ...)                                 \
  template <>                                                          \
  const views::metadata::EnumStrings<T>&                               \
  views::metadata::GetEnumStringsInstance<T>() {                       \
    static const base::NoDestructor<EnumStrings<T>> instance(          \
        std::vector<views::metadata::EnumStrings<T>::EnumString>(      \
            {__VA_ARGS__}));                                           \
    return *instance;                                                  \
  }                                                                    \
                                                                       \
  template <>                                                          \
  base::string16 views::metadata::ConvertToString<T>(T source_value) { \
    for (const auto& pair : GetEnumStringsInstance<T>().pairs) {       \
      if (source_value == pair.enum_value)                             \
        return pair.str_value;                                         \
    }                                                                  \
    return base::string16();                                           \
  }                                                                    \
                                                                       \
  template <>                                                          \
  bool views::metadata::ConvertFromString<T>(                          \
      const base::string16& source_value, T* dst_value) {              \
    for (const auto& pair : GetEnumStringsInstance<T>().pairs) {       \
      if (source_value == pair.str_value) {                            \
        *dst_value = pair.enum_value;                                  \
        return true;                                                   \
      }                                                                \
    }                                                                  \
    return false;                                                      \
  }

// Type Conversion Template Function
// --------------------------------------------
template <typename TSource>
base::string16 ConvertToString(ArgType<TSource> source_value);

template <typename TTarget>
bool ConvertFromString(const base::string16& source_value, TTarget* dst_value);

// String Conversions ---------------------------------------------------------

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

template <>
VIEWS_EXPORT base::string16 ConvertToString<base::string16>(
    const base::string16& source_value);

template <>
VIEWS_EXPORT bool ConvertFromString<int8_t>(const base::string16& source_value,
                                            int8_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<int16_t>(const base::string16& source_value,
                                             int16_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<int32_t>(const base::string16& source_value,
                                             int32_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<int64_t>(const base::string16& source_value,
                                             int64_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<uint8_t>(const base::string16& source_value,
                                             uint8_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<uint16_t>(
    const base::string16& source_value,
    uint16_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<uint32_t>(
    const base::string16& source_value,
    uint32_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<uint64_t>(
    const base::string16& source_value,
    uint64_t* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<double>(const base::string16& source_value,
                                            double* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<float>(const base::string16& source_value,
                                           float* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<bool>(const base::string16& source_value,
                                          bool* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<gfx::Size>(
    const base::string16& source_value,
    gfx::Size* dst_value);

template <>
VIEWS_EXPORT bool ConvertFromString<base::string16>(
    const base::string16& source_value,
    base::string16* dst_value);

}  // namespace metadata
}  // namespace views

#endif  // UI_VIEWS_METADATA_TYPE_CONVERSION_H_
