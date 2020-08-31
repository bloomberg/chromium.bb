// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_STREAMING_RTCP_SESSION_H_
#define CAST_STREAMING_RTCP_SESSION_H_

#include "cast/streaming/ntp_time.h"
#include "cast/streaming/ssrc.h"

namespace openscreen {
namespace cast {

// Session-level configuration and shared components for the RTCP messaging
// associated with a single Cast RTP stream. Multiple packet serialization and
// parsing components share a single RtcpSession instance for data consistency.
class RtcpSession {
 public:
  // |start_time| should be the current time, as it is used by NtpTimeConverter
  // to set a fixed reference point between the local Clock and current "real
  // world" wall time.
  RtcpSession(Ssrc sender_ssrc,
              Ssrc receiver_ssrc,
              Clock::time_point start_time);
  ~RtcpSession();

  Ssrc sender_ssrc() const { return sender_ssrc_; }
  Ssrc receiver_ssrc() const { return receiver_ssrc_; }
  const NtpTimeConverter& ntp_converter() const { return ntp_converter_; }

 private:
  const Ssrc sender_ssrc_;
  const Ssrc receiver_ssrc_;

  // Translates between system time (internal format) and NTP (wire format).
  NtpTimeConverter ntp_converter_;
};

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_STREAMING_RTCP_SESSION_H_
