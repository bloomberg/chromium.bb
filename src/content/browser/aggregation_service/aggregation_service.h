// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_
#define CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_

#include "content/browser/aggregation_service/aggregatable_report_assembler.h"
#include "content/browser/aggregation_service/aggregatable_report_sender.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class Time;
class Value;
}  // namespace base

namespace content {

class AggregatableReport;
class AggregatableReportRequest;
class BrowserContext;

// External interface for the aggregation service.
class AggregationService {
 public:
  using AssemblyStatus = AggregatableReportAssembler::AssemblyStatus;
  using AssemblyCallback = AggregatableReportAssembler::AssemblyCallback;

  using SendStatus = AggregatableReportSender::RequestStatus;
  using SendCallback = AggregatableReportSender::ReportSentCallback;

  virtual ~AggregationService() = default;

  // Gets the AggregationService that should be used for handling aggregations
  // in the given `browser_context`. Returns nullptr if aggregation service is
  // not enabled.
  static AggregationService* GetService(BrowserContext* browser_context);

  // Constructs an AggregatableReport from the information in `report_request`.
  // `callback` will be run once completed which returns the assembled report
  // if successful, otherwise `absl::nullopt` will be returned.
  virtual void AssembleReport(AggregatableReportRequest report_request,
                              AssemblyCallback callback) = 0;

  // Sends an aggregatable report to the reporting endpoint `url`.
  virtual void SendReport(const GURL& url,
                          const AggregatableReport& report,
                          SendCallback callback) = 0;

  // Sends the contents of an aggregatable report to the reporting endpoint
  // `url`. This allows a caller to modify the report's JSON serialization as
  // needed.
  virtual void SendReport(const GURL& url,
                          const base::Value& contents,
                          SendCallback callback) = 0;

  // Deletes all data in storage that were fetched between `delete_begin` and
  // `delete_end` time (inclusive). Null times are treated as unbounded lower or
  // upper range.
  virtual void ClearData(base::Time delete_begin,
                         base::Time delete_end,
                         base::OnceClosure done) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_AGGREGATION_SERVICE_AGGREGATION_SERVICE_H_
