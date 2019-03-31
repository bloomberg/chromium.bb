// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/ntp_time.h"

#include "platform/api/logging.h"

using openscreen::platform::Clock;
using std::chrono::duration_cast;

namespace openscreen {
namespace cast_streaming {

namespace {

// The number of seconds between 1 January 1900 and 1 January 1970.
constexpr NtpSeconds kTimeBetweenNtpAndUnixEpoch{INT64_C(2208988800)};

}  // namespace

NtpTimeConverter::NtpTimeConverter(platform::Clock::time_point now,
                                   std::chrono::seconds since_unix_epoch)
    : start_time_(now),
      since_ntp_epoch_(duration_cast<NtpSeconds>(since_unix_epoch) +
                       kTimeBetweenNtpAndUnixEpoch) {}

NtpTimeConverter::~NtpTimeConverter() = default;

NtpTimestamp NtpTimeConverter::ToNtpTimestamp(
    Clock::time_point time_point) const {
  const Clock::duration time_since_start = time_point - start_time_;
  const auto whole_seconds = duration_cast<NtpSeconds>(time_since_start);
  const auto remainder =
      duration_cast<NtpFraction>(time_since_start - whole_seconds);
  return AssembleNtpTimestamp(since_ntp_epoch_ + whole_seconds, remainder);
}

Clock::time_point NtpTimeConverter::ToLocalTime(NtpTimestamp timestamp) const {
  OSP_DCHECK_LT(NtpSecondsPart(timestamp).count(), INT64_C(4263431296))
      << "One year left to fix the NTP year 2036 wrap-around issue!";
  const auto whole_seconds = NtpSecondsPart(timestamp) - since_ntp_epoch_;
  const auto seconds_since_start =
      duration_cast<Clock::duration>(whole_seconds) + start_time_;
  const auto remainder =
      duration_cast<Clock::duration>(NtpFractionPart(timestamp));
  return seconds_since_start + remainder;
}

}  // namespace cast_streaming
}  // namespace openscreen
