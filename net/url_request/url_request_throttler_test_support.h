// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_THROTTLER_TEST_SUPPORT_H_
#define NET_URL_REQUEST_URL_REQUEST_THROTTLER_TEST_SUPPORT_H_

#include <string>

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_request_throttler_header_interface.h"

namespace net {

class TestTickClock : public base::TickClock {
 public:
  TestTickClock();
  explicit TestTickClock(base::TimeTicks now);
  ~TestTickClock() override;

  base::TimeTicks NowTicks() override;
  void set_now(base::TimeTicks now) { now_ticks_ = now; }

 private:
  base::TimeTicks now_ticks_;
  DISALLOW_COPY_AND_ASSIGN(TestTickClock);
};

// Mocks the URLRequestThrottlerHeaderInterface, allowing testing code to
// pass arbitrary fake headers to the throttling code.
class MockURLRequestThrottlerHeaderAdapter
    : public URLRequestThrottlerHeaderInterface {
 public:
  // Constructs mock response headers with the given |response_code| and no
  // custom response header fields.
  explicit MockURLRequestThrottlerHeaderAdapter(int response_code);

  // Constructs mock response headers with the given |response_code| and
  // with a custom-retry header value |retry_value| if it is non-empty, and
  // a custom opt-out header value |opt_out_value| if it is non-empty.
  MockURLRequestThrottlerHeaderAdapter(const std::string& retry_value,
                                       const std::string& opt_out_value,
                                       int response_code);
  ~MockURLRequestThrottlerHeaderAdapter() override;

  // URLRequestThrottlerHeaderInterface overrides.
  std::string GetNormalizedValue(const std::string& key) const override;
  int GetResponseCode() const override;

 private:
  std::string fake_retry_value_;
  std::string fake_opt_out_value_;
  int fake_response_code_;

  DISALLOW_COPY_AND_ASSIGN(MockURLRequestThrottlerHeaderAdapter);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_THROTTLER_TEST_SUPPORT_H_
