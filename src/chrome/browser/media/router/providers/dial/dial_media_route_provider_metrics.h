// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_METRICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_METRICS_H_

namespace media_router {

static constexpr char kHistogramDialCreateRouteResult[] =
    "MediaRouter.Dial.CreateRoute";
static constexpr char kHistogramDialTerminateRouteResult[] =
    "MediaRouter.Dial.TerminateRoute";
static constexpr char kHistogramDialParseMessageResult[] =
    "MediaRouter.Dial.ParseMessage";

// Note on enums defined in this file:
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DialCreateRouteResult {
  kSuccess = 0,
  kSinkNotFound = 1,
  kAppInfoNotFound = 2,
  kAppLaunchFailed = 3,
  kUnsupportedSource = 4,
  kRouteAlreadyExists = 5,
  kCount
};

enum class DialTerminateRouteResult {
  kSuccess = 0,
  kRouteNotFound = 1,
  kSinkNotFound = 2,
  kStopAppFailed = 3,
  kCount
};

enum class DialParseMessageResult {
  kSuccess = 0,
  kParseError = 1,
  kInvalidMessage = 2,
  kCount
};

class DialMediaRouteProviderMetrics {
 public:
  static void RecordCreateRouteResult(DialCreateRouteResult result);
  static void RecordTerminateRouteResult(DialTerminateRouteResult result);
  static void RecordParseMessageResult(DialParseMessageResult result);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_DIAL_DIAL_MEDIA_ROUTE_PROVIDER_METRICS_H_
