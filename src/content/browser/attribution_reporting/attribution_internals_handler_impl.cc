// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_internals_handler_impl.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

namespace {

using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;
using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

using Attributability =
    ::content::mojom::WebUIAttributionSource::Attributability;

mojom::WebUIAttributionSourcePtr WebUIAttributionSource(
    const StorableSource& source,
    absl::optional<DeactivatedSource::Reason> deactivation_reason) {
  auto attributability = Attributability::kAttributable;
  if (source.attribution_logic() == StorableSource::AttributionLogic::kNever) {
    attributability = Attributability::kNoised;
  } else if (deactivation_reason.has_value()) {
    switch (*deactivation_reason) {
      case DeactivatedSource::Reason::kReplacedByNewerSource:
        attributability = Attributability::kReplacedByNewerSource;
        break;
      case DeactivatedSource::Reason::kReachedAttributionLimit:
        attributability = Attributability::kReachedAttributionLimit;
        break;
    }
  }

  return mojom::WebUIAttributionSource::New(
      source.source_event_id(), source.impression_origin(),
      source.ConversionDestination().Serialize(), source.reporting_origin(),
      source.impression_time().ToJsTime(), source.expiry_time().ToJsTime(),
      source.source_type(), source.priority(), source.dedup_keys(),
      attributability);
}

void ForwardSourcesToWebUI(
    mojom::AttributionInternalsHandler::GetActiveSourcesCallback
        web_ui_callback,
    std::vector<StorableSource> active_sources) {
  std::vector<mojom::WebUIAttributionSourcePtr> web_ui_sources;
  web_ui_sources.reserve(active_sources.size());

  for (const StorableSource& source : active_sources) {
    web_ui_sources.push_back(
        WebUIAttributionSource(source, /*deactivation_reason=*/absl::nullopt));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_sources));
}

mojom::WebUIAttributionReportPtr WebUIAttributionReport(
    const AttributionReport& report,
    int http_response_code,
    mojom::WebUIAttributionReport::Status status) {
  const auto* data =
      absl::get_if<AttributionReport::EventLevelData>(&report.data());
  DCHECK(data);
  return mojom::WebUIAttributionReport::New(
      data->id, report.source().ConversionDestination().Serialize(),
      report.ReportURL(),
      /*trigger_time=*/report.trigger_time().ToJsTime(),
      /*report_time=*/report.report_time().ToJsTime(), data->priority,
      report.ReportBody(/*pretty_print=*/true),
      /*attributed_truthfully=*/report.source().attribution_logic() ==
          StorableSource::AttributionLogic::kTruthfully,
      status, http_response_code);
}

void ForwardReportsToWebUI(
    mojom::AttributionInternalsHandler::GetReportsCallback web_ui_callback,
    std::vector<AttributionReport> pending_reports) {
  std::vector<mojom::WebUIAttributionReportPtr> web_ui_reports;
  web_ui_reports.reserve(pending_reports.size());
  for (const AttributionReport& report : pending_reports) {
    web_ui_reports.push_back(WebUIAttributionReport(
        report, /*http_response_code=*/0,
        mojom::WebUIAttributionReport::Status::kPending));
  }

  std::move(web_ui_callback).Run(std::move(web_ui_reports));
}

}  // namespace

AttributionInternalsHandlerImpl::AttributionInternalsHandlerImpl(
    WebUI* web_ui,
    mojo::PendingReceiver<mojom::AttributionInternalsHandler> receiver)
    : web_ui_(web_ui),
      manager_provider_(std::make_unique<AttributionManagerProviderImpl>()),
      receiver_(this, std::move(receiver)) {}

AttributionInternalsHandlerImpl::~AttributionInternalsHandlerImpl() = default;

void AttributionInternalsHandlerImpl::IsAttributionReportingEnabled(
    mojom::AttributionInternalsHandler::IsAttributionReportingEnabledCallback
        callback) {
  content::WebContents* contents = web_ui_->GetWebContents();
  bool attribution_reporting_enabled =
      manager_provider_->GetManager(contents) &&
      GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
          contents->GetBrowserContext(),
          ContentBrowserClient::ConversionMeasurementOperation::kAny,
          /*impression_origin=*/nullptr, /*conversion_origin=*/nullptr,
          /*reporting_origin=*/nullptr);
  bool debug_mode = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kConversionsDebugMode);
  std::move(callback).Run(attribution_reporting_enabled, debug_mode);
}

void AttributionInternalsHandlerImpl::GetActiveSources(
    mojom::AttributionInternalsHandler::GetActiveSourcesCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->GetActiveSourcesForWebUI(
        base::BindOnce(&ForwardSourcesToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::GetReports(
    mojom::AttributionInternalsHandler::GetReportsCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->GetPendingReportsForWebUI(
        base::BindOnce(&ForwardReportsToWebUI, std::move(callback)));
  } else {
    std::move(callback).Run({});
  }
}

void AttributionInternalsHandlerImpl::SendReports(
    const std::vector<AttributionReport::EventLevelData::Id>& ids,
    mojom::AttributionInternalsHandler::SendReportsCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->SendReportsForWebUI(ids, std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::ClearStorage(
    mojom::AttributionInternalsHandler::ClearStorageCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager->ClearData(base::Time::Min(), base::Time::Max(),
                       base::NullCallback(), std::move(callback));
  } else {
    std::move(callback).Run();
  }
}

void AttributionInternalsHandlerImpl::AddObserver(
    mojo::PendingRemote<mojom::AttributionInternalsObserver> observer,
    mojom::AttributionInternalsHandler::AddObserverCallback callback) {
  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    observers_.Add(std::move(observer));

    if (!manager_observation_.IsObservingSource(manager))
      manager_observation_.Observe(manager);

    std::move(callback).Run(true);
  } else {
    std::move(callback).Run(false);
  }
}

void AttributionInternalsHandlerImpl::OnSourcesChanged() {
  for (auto& observer : observers_)
    observer->OnSourcesChanged();
}

void AttributionInternalsHandlerImpl::OnReportsChanged() {
  for (auto& observer : observers_)
    observer->OnReportsChanged();
}

void AttributionInternalsHandlerImpl::OnSourceDeactivated(
    const AttributionStorage::DeactivatedSource& deactivated_source) {
  auto source = WebUIAttributionSource(deactivated_source.source,
                                       deactivated_source.reason);

  for (auto& observer : observers_) {
    observer->OnSourceDeactivated(source.Clone());
  }
}

void AttributionInternalsHandlerImpl::OnReportSent(
    const AttributionReport& report,
    const SendResult& info) {
  mojom::WebUIAttributionReport::Status status;
  switch (info.status) {
    case SendResult::Status::kSent:
      status = mojom::WebUIAttributionReport::Status::kSent;
      break;
    case SendResult::Status::kDropped:
      status =
          mojom::WebUIAttributionReport::Status::kProhibitedByBrowserPolicy;
      break;
    case SendResult::Status::kFailure:
      status = mojom::WebUIAttributionReport::Status::kNetworkError;
      break;
    case SendResult::Status::kTransientFailure:
      NOTREACHED();
      return;
  }

  auto web_report =
      WebUIAttributionReport(report, info.http_response_code, status);

  for (auto& observer : observers_) {
    observer->OnReportSent(web_report.Clone());
  }
}

void AttributionInternalsHandlerImpl::OnReportDropped(
    const AttributionStorage::CreateReportResult& result) {
  mojom::WebUIAttributionReport::Status status;
  switch (result.status()) {
    case CreateReportStatus::kSuccessDroppedLowerPriority:
    case CreateReportStatus::kPriorityTooLow:
      status = mojom::WebUIAttributionReport::Status::kDroppedDueToLowPriority;
      break;
    case CreateReportStatus::kDroppedForNoise:
      status = mojom::WebUIAttributionReport::Status::kDroppedForNoise;
      break;
    case CreateReportStatus::kRateLimited:
      status = mojom::WebUIAttributionReport::Status::kDroppedDueToRateLimiting;
      break;
    default:
      NOTREACHED();
      return;
  }

  auto report = WebUIAttributionReport(*result.dropped_report(),
                                       /*http_response_code=*/0, status);

  for (auto& observer : observers_) {
    observer->OnReportDropped(report.Clone());
  }
}

void AttributionInternalsHandlerImpl::SetAttributionManagerProviderForTesting(
    std::unique_ptr<AttributionManager::Provider> manager_provider) {
  DCHECK(manager_provider);

  manager_observation_.Reset();
  manager_provider_ = std::move(manager_provider);

  if (AttributionManager* manager =
          manager_provider_->GetManager(web_ui_->GetWebContents())) {
    manager_observation_.Observe(manager);
  }
}

}  // namespace content
