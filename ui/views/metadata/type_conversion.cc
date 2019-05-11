// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
namespace metadata {

/***** String Conversions *****/

template <>
base::string16 ConvertToString<base::string16>(
    const base::string16& source_value) {
  return source_value;
}

template <>
base::string16 ConvertToString<int8_t>(int8_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<int16_t>(int16_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<int32_t>(int32_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<int64_t>(int64_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint8_t>(uint8_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint16_t>(uint16_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint32_t>(uint32_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint64_t>(uint64_t source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<float>(float source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<double>(double source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<bool>(bool source_value) {
  return base::ASCIIToUTF16(source_value ? "true" : "false");
}

template <>
base::string16 ConvertToString<gfx::Size>(const gfx::Size& source_value) {
  return base::ASCIIToUTF16(base::StringPrintf("{%i, %i}", source_value.width(),
                                               source_value.height()));
}

template <>
int8_t ConvertFromString<int8_t>(const base::string16& source_value,
                                 int8_t default_value) {
  int32_t ret = 0;
  return base::StringToInt(source_value, &ret) &&
                 base::IsValueInRangeForNumericType<int8_t>(ret)
             ? static_cast<int8_t>(ret)
             : default_value;
}

template <>
int16_t ConvertFromString<int16_t>(const base::string16& source_value,
                                   int16_t default_value) {
  int32_t ret = 0;
  return base::StringToInt(source_value, &ret) &&
                 base::IsValueInRangeForNumericType<int16_t>(ret)
             ? static_cast<int16_t>(ret)
             : default_value;
}

template <>
int32_t ConvertFromString<int32_t>(const base::string16& source_value,
                                   int32_t default_value) {
  int32_t ret = 0;
  return base::StringToInt(source_value, &ret) ? static_cast<int32_t>(ret)
                                               : default_value;
}

template <>
int64_t ConvertFromString<int64_t>(const base::string16& source_value,
                                   int64_t default_value) {
  int64_t ret = 0;
  return base::StringToInt64(source_value, &ret) ? ret : default_value;
}

template <>
uint8_t ConvertFromString<uint8_t>(const base::string16& source_value,
                                   uint8_t default_value) {
  uint32_t ret = 0;
  return base::StringToUint(source_value, &ret) &&
                 base::IsValueInRangeForNumericType<uint8_t>(ret)
             ? static_cast<uint8_t>(ret)
             : default_value;
}

template <>
uint16_t ConvertFromString<uint16_t>(const base::string16& source_value,
                                     uint16_t default_value) {
  uint32_t ret = 0;
  return base::StringToUint(source_value, &ret) &&
                 base::IsValueInRangeForNumericType<uint16_t>(ret)
             ? static_cast<uint16_t>(ret)
             : default_value;
}

template <>
uint32_t ConvertFromString<uint32_t>(const base::string16& source_value,
                                     uint32_t default_value) {
  uint32_t ret = 0;
  return base::StringToUint(source_value, &ret) ? static_cast<uint32_t>(ret)
                                                : default_value;
}

template <>
uint64_t ConvertFromString<uint64_t>(const base::string16& source_value,
                                     uint64_t default_value) {
  uint64_t ret = 0;
  return base::StringToUint64(source_value, &ret) ? ret : default_value;
}

template <>
float ConvertFromString<float>(const base::string16& source_value,
                               float default_value) {
  return static_cast<float>(
      ConvertFromString<double>(source_value, default_value));
}

template <>
double ConvertFromString<double>(const base::string16& source_value,
                                 double default_value) {
  double ret = 0;
  return base::StringToDouble(base::UTF16ToUTF8(source_value), &ret)
             ? ret
             : default_value;
}

template <>
bool ConvertFromString<bool>(const base::string16& source_value,
                             bool default_value) {
  if (source_value == base::ASCIIToUTF16("false"))
    return false;
  return source_value == base::ASCIIToUTF16("true") || default_value;
}

template <>
gfx::Size ConvertFromString<gfx::Size>(const base::string16& source_value,
                                       const gfx::Size& default_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("{,}"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int width, height;
  return (values.size() == 2) && base::StringToInt(values[0], &width) &&
                 base::StringToInt(values[1], &height)
             ? gfx::Size(width, height)
             : default_value;
}

}  // namespace metadata
}  // namespace views
