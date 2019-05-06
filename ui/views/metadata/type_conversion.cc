// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/metadata/type_conversion.h"

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
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
base::string16 ConvertToString<int8_t>(const int8_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<int16_t>(const int16_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<int32_t>(const int32_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<int64_t>(const int64_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint8_t>(const uint8_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint16_t>(const uint16_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint32_t>(const uint32_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<uint64_t>(const uint64_t& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<float>(const float& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<double>(const double& source_value) {
  return base::NumberToString16(source_value);
}

template <>
base::string16 ConvertToString<bool>(const bool& source_value) {
  return source_value ? base::ASCIIToUTF16("true")
                      : base::ASCIIToUTF16("false");
}

template <>
base::string16 ConvertToString<gfx::Size>(const gfx::Size& source_value) {
  return base::ASCIIToUTF16(base::StringPrintf("{%i, %i}", source_value.width(),
                                               source_value.height()));
}

template <>
int8_t ConvertFromString<int8_t>(const base::string16& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret))
    return static_cast<int8_t>(ret);
  NOTREACHED();
  return 0;
}

template <>
int16_t ConvertFromString<int16_t>(const base::string16& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret))
    return static_cast<int16_t>(ret);
  NOTREACHED();
  return 0;
}

template <>
int32_t ConvertFromString<int32_t>(const base::string16& source_value) {
  int32_t ret = 0;
  if (base::StringToInt(source_value, &ret))
    return ret;
  NOTREACHED();
  return 0;
}

template <>
int64_t ConvertFromString<int64_t>(const base::string16& source_value) {
  int64_t ret = 0;
  if (base::StringToInt64(source_value, &ret))
    return ret;
  NOTREACHED();
  return 0;
}

template <>
uint8_t ConvertFromString<uint8_t>(const base::string16& source_value) {
  uint32_t ret = 0;
  if (base::StringToUint(source_value, &ret))
    return static_cast<uint8_t>(ret);
  NOTREACHED();
  return 0;
}

template <>
uint16_t ConvertFromString<uint16_t>(const base::string16& source_value) {
  uint32_t ret = 0;
  if (base::StringToUint(source_value, &ret))
    return static_cast<uint16_t>(ret);
  NOTREACHED();
  return 0;
}

template <>
uint32_t ConvertFromString<uint32_t>(const base::string16& source_value) {
  uint32_t ret = 0;
  if (base::StringToUint(source_value, &ret))
    return ret;
  NOTREACHED();
  return 0;
}

template <>
uint64_t ConvertFromString<uint64_t>(const base::string16& source_value) {
  uint64_t ret = 0;
  if (base::StringToUint64(source_value, &ret))
    return ret;
  NOTREACHED();
  return 0;
}

template <>
double ConvertFromString<double>(const base::string16& source_value) {
  double ret = 0;
  if (base::StringToDouble(base::UTF16ToUTF8(source_value), &ret))
    return ret;
  NOTREACHED();
  return 0.0;
}

template <>
float ConvertFromString<float>(const base::string16& source_value) {
  return static_cast<float>(ConvertFromString<double>(source_value));
}

template <>
bool ConvertFromString<bool>(const base::string16& source_value) {
  if (source_value == base::ASCIIToUTF16("true"))
    return true;
  if (source_value == base::ASCIIToUTF16("false"))
    return false;

  NOTREACHED();
  return false;
}

}  // namespace metadata
}  // namespace views
