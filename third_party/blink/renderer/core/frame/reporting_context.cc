// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/reporting_context.h"

#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_observer.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
const char ReportingContext::kSupplementName[] = "ReportingContext";

ReportingContext::ReportingContext(ExecutionContext& context)
    : Supplement<ExecutionContext>(context), execution_context_(context) {}

// static
ReportingContext* ReportingContext::From(ExecutionContext* context) {
  ReportingContext* reporting_context =
      Supplement<ExecutionContext>::From<ReportingContext>(context);
  if (!reporting_context) {
    reporting_context = MakeGarbageCollected<ReportingContext>(*context);
    Supplement<ExecutionContext>::ProvideTo(*context, reporting_context);
  }
  return reporting_context;
}

void ReportingContext::QueueReport(Report* report) {
  CountReport(report);

  // Buffer the report.
  if (!report_buffer_.Contains(report->type()))
    report_buffer_.insert(report->type(), HeapListHashSet<Member<Report>>());
  report_buffer_.find(report->type())->value.insert(report);

  // Only the most recent 100 reports will remain buffered, per report type.
  // https://w3c.github.io/reporting/#notify-observers
  if (report_buffer_.at(report->type()).size() > 100)
    report_buffer_.find(report->type())->value.RemoveFirst();

  for (auto observer : observers_)
    observer->QueueReport(report);
}

void ReportingContext::RegisterObserver(ReportingObserver* observer) {
  UseCounter::Count(execution_context_, WebFeature::kReportingObserver);

  observers_.insert(observer);
  if (!observer->Buffered())
    return;

  observer->ClearBuffered();
  for (auto type : report_buffer_) {
    for (Report* report : type.value) {
      observer->QueueReport(report);
    }
  }
}

void ReportingContext::UnregisterObserver(ReportingObserver* observer) {
  observers_.erase(observer);
}

const mojom::blink::ReportingServiceProxyPtr&
ReportingContext::GetReportingService() const {
  if (!reporting_service_) {
    Platform::Current()->GetConnector()->BindInterface(
        Platform::Current()->GetBrowserServiceName(), &reporting_service_);
  }
  return reporting_service_;
}

void ReportingContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(observers_);
  visitor->Trace(report_buffer_);
  visitor->Trace(execution_context_);
  Supplement<ExecutionContext>::Trace(visitor);
}

void ReportingContext::CountReport(Report* report) {
  const String& type = report->type();
  WebFeature feature;

  if (type == "deprecation") {
    feature = WebFeature::kDeprecationReport;
  } else if (type == "feature-policy") {
    feature = WebFeature::kFeaturePolicyReport;
  } else if (type == "intervention") {
    feature = WebFeature::kInterventionReport;
  } else {
    return;
  }

  UseCounter::Count(execution_context_, feature);
}

}  // namespace blink
