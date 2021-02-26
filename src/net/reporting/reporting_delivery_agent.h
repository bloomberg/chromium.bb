// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_DELIVERY_AGENT_H_
#define NET_REPORTING_REPORTING_DELIVERY_AGENT_H_

#include <memory>

#include "base/macros.h"
#include "net/base/net_export.h"
#include "net/base/rand_callback.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace net {

class ReportingContext;

// Batches reports fetched from the ReportingCache and uploads them using the
// ReportingUploader.
//
// Reports are only considered for delivery if all of the following are true:
//  - The report is not already part of a pending upload request.
//  - Uploads are allowed for the report's origin (i.e. the origin of the URL
//    associated with the reported event).
//  - There is not already a pending upload for any reports sharing the same
//    (NIK, origin, group) key.
//
// Reports are batched for upload to an endpoint URL such that:
//  - The available reports with the same (NIK, origin, group) are always
//    uploaded together.
//  - All reports uploaded together must share a NIK and origin.
//  - Reports for the same (NIK, origin) can be uploaded separately if they are
//    for different groups.
//  - Reports for different groups can be batched together, if they are assigned
//    to ReportingEndpoints sharing a URL (that is, the upload URL).
//
// There is no limit to the number of reports that can be uploaded together.
// (Aside from the global cap on total reports.)
//
// TODO(juliatuttle): Consider capping the maximum number of reports per
// delivery attempt.
class NET_EXPORT ReportingDeliveryAgent {
 public:
  // Creates a ReportingDeliveryAgent. |context| must outlive the agent.
  static std::unique_ptr<ReportingDeliveryAgent> Create(
      ReportingContext* context,
      const RandIntCallback& rand_callback);

  virtual ~ReportingDeliveryAgent();

  // Replaces the internal OneShotTimer used for scheduling report delivery
  // attempts with a caller-specified one so that unittests can provide a
  // MockOneShotTimer.
  virtual void SetTimerForTesting(
      std::unique_ptr<base::OneShotTimer> timer) = 0;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_DELIVERY_AGENT_H_
