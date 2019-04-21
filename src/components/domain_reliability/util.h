// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_
#define COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "components/domain_reliability/uploader.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_status.h"

namespace base {
class Location;
}

namespace domain_reliability {

// Attempts to convert a net error and an HTTP response code into the status
// string that should be recorded in a beacon. Returns true if it could.
//
// N.B.: This functions as the whitelist of "safe" errors to report; network-
//       local errors are purposefully not converted to avoid revealing
//       information about the local network to the remote server.
bool GetDomainReliabilityBeaconStatus(
    int net_error,
    int http_response_code,
    std::string* beacon_status_out);

std::string GetDomainReliabilityProtocol(
    net::HttpResponseInfo::ConnectionInfo connection_info,
    bool ssl_info_populated);

// Converts a URLRequestStatus into a network error. Returns the error code for
// FAILED; maps SUCCESS and CANCELED to OK and ERR_ABORTED, respectively; and
// returns ERR_ABORTED for any other status.
int GetNetErrorFromURLRequestStatus(const net::URLRequestStatus& status);

// Based on the network error code, HTTP response code, and Retry-After value,
// fills |status| with the result of a report upload.
void GetUploadResultFromResponseDetails(
    int net_error,
    int http_response_code,
    base::TimeDelta retry_after,
    DomainReliabilityUploader::UploadResult* result);

GURL SanitizeURLForReport(
    const GURL& beacon_url,
    const GURL& collector_url,
    const std::vector<std::unique_ptr<std::string>>& path_prefixes);

// Mockable wrapper around TimeTicks::Now and Timer. Mock version is in
// test_util.h.
// TODO(juliatuttle): Rename to Time{Provider,Source,?}.
class DOMAIN_RELIABILITY_EXPORT MockableTime : public base::Clock,
                                               public base::TickClock {
 public:
  // Mockable wrapper around (a subset of) base::Timer.
  class DOMAIN_RELIABILITY_EXPORT Timer {
   public:
    virtual ~Timer();

    virtual void Start(const base::Location& posted_from,
                       base::TimeDelta delay,
                       const base::Closure& user_task) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() = 0;

   protected:
    Timer();
  };

  ~MockableTime() override;

  // Clock impl; returns base::Time::Now() or a mocked version thereof.
  base::Time Now() const override = 0;
  // TickClock impl; returns base::TimeTicks::Now() or a mocked version thereof.
  base::TimeTicks NowTicks() const override = 0;

  // Returns a new Timer, or a mocked version thereof.
  virtual std::unique_ptr<MockableTime::Timer> CreateTimer() = 0;

 protected:
  MockableTime();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockableTime);
};

// Implementation of MockableTime that passes through to
// base::Time{,Ticks}::Now() and base::Timer.
class DOMAIN_RELIABILITY_EXPORT ActualTime : public MockableTime {
 public:
  ActualTime();

  ~ActualTime() override;

  // MockableTime implementation:
  base::Time Now() const override;
  base::TimeTicks NowTicks() const override;
  std::unique_ptr<MockableTime::Timer> CreateTimer() override;
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_UTIL_H_
