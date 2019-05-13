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
base::string16 ConvertToString<base::string16>(
    const base::string16& source_value) {
  return source_value;
}

template <>
bool ConvertFromString<int8_t>(const base::string16& source_value,
                               int8_t* dst_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int8_t>(ret)) {
    *dst_value = static_cast<int8_t>(ret);
    return true;
  }
  return false;
}

template <>
bool ConvertFromString<int16_t>(const base::string16& source_value,
                                int16_t* dst_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret) &&
      base::IsValueInRangeForNumericType<int16_t>(ret)) {
    *dst_value = static_cast<int16_t>(ret);
    return true;
  }
  return false;
}

template <>
bool ConvertFromString<int32_t>(const base::string16& source_value,
                                int32_t* dst_value) {
  return base::StringToInt(source_value, dst_value);
}

template <>
bool ConvertFromString<int64_t>(const base::string16& source_value,
                                int64_t* dst_value) {
  return base::StringToInt64(source_value, dst_value);
}

template <>
bool ConvertFromString<uint8_t>(const base::string16& source_value,
                                uint8_t* dst_value) {
  uint32_t ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint8_t>(ret)) {
    *dst_value = static_cast<uint8_t>(ret);
    return true;
  }
  return false;
}

template <>
bool ConvertFromString<uint16_t>(const base::string16& source_value,
                                 uint16_t* dst_value) {
  uint32_t ret = 0;
  if (base::StringToUint(source_value, &ret) &&
      base::IsValueInRangeForNumericType<uint16_t>(ret)) {
    *dst_value = static_cast<uint16_t>(ret);
    return true;
  }
  return false;
}

template <>
bool ConvertFromString<uint32_t>(const base::string16& source_value,
                                 uint32_t* dst_value) {
  return base::StringToUint(source_value, dst_value);
}

template <>
bool ConvertFromString<uint64_t>(const base::string16& source_value,
                                 uint64_t* dst_value) {
  return base::StringToUint64(source_value, dst_value);
}

template <>
bool ConvertFromString<float>(const base::string16& source_value,
                              float* dst_value) {
  double temp;
  if (ConvertFromString<double>(source_value, &temp)) {
    *dst_value = static_cast<float>(temp);
    return true;
  }
  return false;
}

template <>
bool ConvertFromString<double>(const base::string16& source_value,
                               double* dst_value) {
  return base::StringToDouble(base::UTF16ToUTF8(source_value), dst_value);
}

template <>
bool ConvertFromString<bool>(const base::string16& source_value,
                             bool* dst_value) {
  if (source_value == base::ASCIIToUTF16("true") ||
      source_value == base::ASCIIToUTF16("false")) {
    *dst_value = source_value == base::ASCIIToUTF16("true");
    return true;
  }
  return false;
}

template <>
bool ConvertFromString<gfx::Size>(const base::string16& source_value,
                                  gfx::Size* dst_value) {
  const auto values =
      base::SplitStringPiece(source_value, base::ASCIIToUTF16("{,}"),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  int width, height;
  if ((values.size() == 2) && base::StringToInt(values[0], &width) &&
      base::StringToInt(values[1], &height)) {
    *dst_value = gfx::Size(width, height);
    return true;
  }
  return false;
}

template <>
bool ConvertFromString<base::string16>(const base::string16& source_value,
                                       base::string16* dst_value) {
  *dst_value = source_value;
  return true;
}

}  // namespace metadata
}  // namespace views
