// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_TYPES_H_
#define CAST_COMMON_CERTIFICATE_TYPES_H_

#include <stdint.h>

namespace cast {
namespace certificate {

struct ConstDataSpan {
  const uint8_t* data;
  uint32_t length;
};

struct DateTime {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
};

bool operator<(const DateTime& a, const DateTime& b);
bool operator>(const DateTime& a, const DateTime& b);
bool DateTimeFromSeconds(uint64_t seconds, DateTime* time);

}  // namespace certificate
}  // namespace cast

#endif  // CAST_COMMON_CERTIFICATE_TYPES_H_
