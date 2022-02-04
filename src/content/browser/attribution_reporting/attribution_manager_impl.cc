// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_manager_impl.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "content/browser/attribution_reporting/attribution_network_sender_impl.h"
#include "content/browser/attribution_reporting/attribution_policy.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_storage_sql.h"
#include "content/browser/attribution_reporting/attribution_utils.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/storable_source.h"
#include "content/browser/attribution_reporting/storable_trigger.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

namespace {

using CreateReportResult = ::content::AttributionStorage::CreateReportResult;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ConversionReportSendOutcome {
  kSent = 0,
  kFailed = 1,
  kDropped = 2,
  kMaxValue = kDropped
};

// The shared-task runner for all attribution storage operations. Note that
// different AttributionManagerImpl perform operations on the same task
// runner. This prevents any potential races when a given context is destroyed
// and recreated for the same backing storage. This uses
// BLOCK_SHUTDOWN as some data deletion operations may be running when the
// browser is closed, and we want to ensure all data is deleted correctly.
base::LazyThreadPoolSequencedTaskRunner g_storage_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                         base::MayBlock(),
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

bool IsOriginSessionOnly(
    scoped_refptr<storage::SpecialStoragePolicy> storage_policy,
    const url::Origin& origin) {
  // TODO(johnidel): This conversion is unfortunate but necessary. Storage
  // partition clear data logic uses Origin keyed deletion, while the storage
  // policy uses GURLs. Ideally these would be coalesced.
  const GURL& url = origin.GetURL();
  if (storage_policy->IsStorageProtected(url))
    return false;

  if (storage_policy->IsStorageSessionOnly(url))
    return true;
  return false;
}

void RecordCreateReportStatus(CreateReportResult::Status status) {
  base::UmaHistogramEnumeration("Conversions.CreateReportStatus", status);
}

// We measure this in order to be able to count reports that weren't
// successfully deleted, which can lead to duplicate reports.
void RecordDeleteEvent(AttributionManagerImpl::DeleteEvent event) {
  base::UmaHistogramEnumeration("Conversions.DeleteSentReportOperation", event);
}

ConversionReportSendOutcome ConvertToConversionReportSendOutcome(
    SendResult::Status status) {
  switch (status) {
    case SendResult::Status::kSent:
      return ConversionReportSendOutcome::kSent;
    case SendResult::Status::kTransientFailure:
    case SendResult::Status::kFailure:
      return ConversionReportSendOutcome::kFailed;
    case SendResult::Status::kDropped:
      return ConversionReportSendOutcome::kDropped;
  }
}

// Called when |report| is to be sent over network, for logging metrics.
void LogMetricsOnReportSend(const AttributionReport& report, base::Time now) {
  // Use a large time range to capture users that might not open the browser for
  // a long time while a conversion report is pending. Revisit this range if it
  // is non-ideal for real world data.
  base::Time original_report_time =
      ComputeReportTime(report.source(), report.trigger_time());
  base::TimeDelta time_since_original_report_time = now - original_report_time;
  base::UmaHistogramCustomTimes(
      "Conversions.ExtraReportDelay2", time_since_original_report_time,
      base::Seconds(1), base::Days(24), /*buckets=*/100);

  base::TimeDelta time_from_conversion_to_report_send =
      report.report_time() - report.trigger_time();
  UMA_HISTOGRAM_COUNTS_1000("Conversions.TimeFromConversionToReportSend",
                            time_from_conversion_to_report_send.InHours());
}

bool IsOffline() {
  return content::GetNetworkConnectionTracker()->IsOffline();
}

}  // namespace

AttributionManager* AttributionManagerProviderImpl::GetManager(
    WebContents* web_contents) const {
  return static_cast<StoragePartitionImpl*>(
             web_contents->GetBrowserContext()->GetDefaultStoragePartition())
      ->GetAttributionManager();
}

// static
void AttributionManagerImpl::RunInMemoryForTesting() {
  AttributionStorageSql::RunInMemoryForTesting();
}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy)
    : AttributionManagerImpl(
          storage_partition,
          user_data_directory,
          std::make_unique<AttributionPolicy>(
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kConversionsDebugMode)),
          std::move(special_storage_policy)) {}

AttributionManagerImpl::AttributionManagerImpl(
    StoragePartitionImpl* storage_partition,
    const base::FilePath& user_data_directory,
    std::unique_ptr<AttributionPolicy> policy,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    std::unique_ptr<NetworkSender> network_sender)
    : storage_partition_(storage_partition),
      attribution_storage_(base::SequenceBound<AttributionStorageSql>(
          g_storage_task_runner.Get(),
          user_data_directory,
          std::make_unique<AttributionStorageDelegateImpl>(
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kConversionsDebugMode)))),
      attribution_policy_(std::move(policy)),
      special_storage_policy_(std::move(special_storage_policy)),
      network_sender_(network_sender
                          ? std::move(network_sender)
                          : std::make_unique<AttributionNetworkSenderImpl>(
                                storage_partition)),
      weak_factory_(this) {
  DCHECK(storage_partition_);
  DCHECK(attribution_policy_);
  DCHECK(network_sender_);

  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  OnConnectionChanged(network::mojom::ConnectionType::CONNECTION_UNKNOWN);
}

AttributionManagerImpl::~AttributionManagerImpl() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);

  // Browser contexts are not required to have a special storage policy.
  if (!special_storage_policy_ ||
      !special_storage_policy_->HasSessionOnlyOrigins()) {
    return;
  }

  // Delete stored data for all session only origins given by
  // |special_storage_policy|.
  base::RepeatingCallback<bool(const url::Origin&)>
      session_only_origin_predicate = base::BindRepeating(
          &IsOriginSessionOnly, std::move(special_storage_policy_));
  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(base::Time::Min(), base::Time::Max(),
                std::move(session_only_origin_predicate));
}

void AttributionManagerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AttributionManagerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AttributionManagerImpl::HandleSource(StorableSource source) {
  // Process attributions in the order in which they were received, processing
  // the new one after any background attributions are flushed.
  GetContentClient()->browser()->FlushBackgroundAttributions(
      base::BindOnce(&AttributionManagerImpl::HandleSourceInternal,
                     weak_factory_.GetWeakPtr(), std::move(source)));
}

void AttributionManagerImpl::HandleSourceInternal(StorableSource source) {
  // Only retrieve deactivated sources if an observer is there to hear it.
  // Technically, an observer could be registered between the time the async
  // call is made and the time the response is received, but this is unlikely.
  int deactivated_source_return_limit = observers_.empty() ? 0 : 50;
  attribution_storage_.AsyncCall(&AttributionStorage::StoreSource)
      .WithArgs(std::move(source), deactivated_source_return_limit)
      .Then(base::BindOnce(
          [](base::WeakPtr<AttributionManagerImpl> manager,
             std::vector<AttributionStorage::DeactivatedSource>
                 deactivated_sources) {
            if (!manager)
              return;

            manager->NotifySourcesChanged();

            for (const auto& source : deactivated_sources) {
              manager->NotifySourceDeactivated(source);
            }
          },
          weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::HandleTrigger(StorableTrigger trigger) {
  GetContentClient()->browser()->FlushBackgroundAttributions(
      base::BindOnce(&AttributionManagerImpl::HandleTriggerInternal,
                     weak_factory_.GetWeakPtr(), std::move(trigger)));
}

void AttributionManagerImpl::HandleTriggerInternal(StorableTrigger trigger) {
  attribution_storage_.AsyncCall(&AttributionStorage::MaybeCreateAndStoreReport)
      .WithArgs(std::move(trigger))
      .Then(base::BindOnce(&AttributionManagerImpl::OnReportStored,
                           weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::OnReportStored(CreateReportResult result) {
  RecordCreateReportStatus(result.status());

  UpdateGetReportsToSendTimer(result.report_time());

  if (result.status() != CreateReportResult::Status::kInternalError) {
    // Sources are changed here because storing a report can cause sources to be
    // deleted or become associated with a dedup key.
    NotifySourcesChanged();
    NotifyReportsChanged();
  }

  if (!result.dropped_report().has_value())
    return;

  if (absl::optional<AttributionStorage::DeactivatedSource> source =
          result.GetDeactivatedSource()) {
    NotifySourceDeactivated(*source);
  }

  for (Observer& observer : observers_)
    observer.OnReportDropped(result);
}

void AttributionManagerImpl::GetActiveSourcesForWebUI(
    base::OnceCallback<void(std::vector<StorableSource>)> callback) {
  const int kMaxSources = 1000;
  attribution_storage_.AsyncCall(&AttributionStorage::GetActiveSources)
      .WithArgs(kMaxSources)
      .Then(std::move(callback));
}

void AttributionManagerImpl::GetPendingReportsForWebUI(
    base::OnceCallback<void(std::vector<AttributionReport>)> callback) {
  GetAndHandleReports(std::move(callback),
                      /*max_report_time=*/base::Time::Max(), /*limit=*/1000);
}

void AttributionManagerImpl::SendReportsForWebUI(
    const std::vector<AttributionReport::EventLevelData::Id>& ids,
    base::OnceClosure done) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetReports)
      .WithArgs(ids)
      .Then(base::BindOnce(&AttributionManagerImpl::OnGetReportsToSendFromWebUI,
                           weak_factory_.GetWeakPtr(), std::move(done)));
}

const AttributionPolicy& AttributionManagerImpl::GetAttributionPolicy() const {
  return *attribution_policy_;
}

void AttributionManagerImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    base::OnceClosure done) {
  attribution_storage_.AsyncCall(&AttributionStorage::ClearData)
      .WithArgs(delete_begin, delete_end, std::move(filter))
      .Then(base::BindOnce(
          [](base::OnceClosure done,
             base::WeakPtr<AttributionManagerImpl> manager) {
            std::move(done).Run();

            if (manager) {
              manager->StartGetReportsToSendTimer();
              manager->NotifySourcesChanged();
              manager->NotifyReportsChanged();
            }
          },
          std::move(done), weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::OnConnectionChanged(
    network::mojom::ConnectionType connection_type) {
  if (IsOffline()) {
    get_reports_to_send_timer_.Stop();
  } else if (absl::optional<AttributionPolicy::OfflineReportDelayConfig> delay =
                 attribution_policy_->GetOfflineReportDelayConfig()) {
    DCHECK(!get_reports_to_send_timer_.IsRunning());

    // Add delay to all reports that should have been sent while the browser was
    // offline so they are not temporally joinable. We do this in storage to
    // avoid pulling an unbounded number of reports into memory, only to
    // immediately issue async storage calls to modify their report times.
    attribution_storage_
        .AsyncCall(&AttributionStorage::AdjustOfflineReportTimes)
        .WithArgs(delay->min, delay->max)
        .Then(
            base::BindOnce(&AttributionManagerImpl::UpdateGetReportsToSendTimer,
                           weak_factory_.GetWeakPtr()));
  }
}

void AttributionManagerImpl::GetAndHandleReports(
    ReportsHandlerFunc handler_function,
    base::Time max_report_time,
    int limit) {
  attribution_storage_.AsyncCall(&AttributionStorage::GetAttributionsToReport)
      .WithArgs(max_report_time, limit)
      .Then(std::move(handler_function));
}

void AttributionManagerImpl::UpdateGetReportsToSendTimer(
    absl::optional<base::Time> time) {
  if (!time.has_value() || IsOffline())
    return;

  if (!get_reports_to_send_timer_.IsRunning() ||
      *time < get_reports_to_send_timer_.desired_run_time()) {
    get_reports_to_send_timer_.Start(FROM_HERE, *time, this,
                                     &AttributionManagerImpl::GetReportsToSend);
  }
}

void AttributionManagerImpl::StartGetReportsToSendTimer() {
  if (IsOffline())
    return;

  attribution_storage_.AsyncCall(&AttributionStorage::GetNextReportTime)
      .WithArgs(base::Time::Now())
      .Then(base::BindOnce(&AttributionManagerImpl::UpdateGetReportsToSendTimer,
                           weak_factory_.GetWeakPtr()));
}

void AttributionManagerImpl::GetReportsToSend() {
  DCHECK(!IsOffline());

  // We only get the next report time strictly after now, because if we are
  // sending a report now but haven't finished doing so and it is still present
  // in storage, storage will return the report time for the same report.
  // Deduplication via `reports_being_sent_` will ensure that the report isn't
  // sent twice, but it will result in wasted processing.
  //
  // TODO(apaseltiner): Consider limiting the number of reports being sent at
  // once, to avoid pulling an arbitrary number of reports into memory.
  GetAndHandleReports(
      base::BindOnce(&AttributionManagerImpl::OnGetReportsToSend,
                     weak_factory_.GetWeakPtr()),
      /*max_report_time=*/base::Time::Now(), /*limit=*/-1);
}

void AttributionManagerImpl::OnGetReportsToSend(
    std::vector<AttributionReport> reports) {
  if (reports.empty() || IsOffline())
    return;

  // Shuffle new reports to provide plausible deniability on the ordering of
  // reports that share the same |report_time|. This is important because
  // multiple conversions for the same impression share the same report time if
  // they are within the same reporting window, and we do not want to allow
  // ordering on their conversion metadata bits.
  base::RandomShuffle(reports.begin(), reports.end());

  SendReports(std::move(reports), /*log_metrics=*/true, base::DoNothing());
  StartGetReportsToSendTimer();
}

void AttributionManagerImpl::OnGetReportsToSendFromWebUI(
    base::OnceClosure done,
    std::vector<AttributionReport> reports) {
  if (reports.empty() || IsOffline()) {
    std::move(done).Run();
    return;
  }

  base::Time now = base::Time::Now();
  for (AttributionReport& report : reports) {
    report.set_report_time(now);
  }

  auto barrier = base::BarrierClosure(reports.size(), std::move(done));
  SendReports(std::move(reports), /*log_metrics=*/false, std::move(barrier));
}

void AttributionManagerImpl::SendReports(std::vector<AttributionReport> reports,
                                         bool log_metrics,
                                         base::RepeatingClosure done) {
  const base::Time now = base::Time::Now();
  for (AttributionReport& report : reports) {
    DCHECK(report.ReportId().has_value());
    DCHECK_LE(report.report_time(), now);

    DCHECK(absl::holds_alternative<AttributionReport::EventLevelData::Id>(
        *report.ReportId()));
    bool inserted =
        reports_being_sent_
            .emplace(absl::get<AttributionReport::EventLevelData::Id>(
                *report.ReportId()))
            .second;
    if (!inserted) {
      done.Run();
      continue;
    }

    bool allowed =
        GetContentClient()->browser()->IsConversionMeasurementOperationAllowed(
            storage_partition_->browser_context(),
            ContentBrowserClient::ConversionMeasurementOperation::kReport,
            &report.source().impression_origin(),
            &report.source().conversion_origin(),
            &report.source().reporting_origin());
    if (!allowed) {
      // If measurement is disallowed, just drop the report on the floor. We
      // need to make sure we forward that the report was "sent" to ensure it is
      // deleted from storage, etc. This simulates sending the report through a
      // null channel.
      OnReportSent(done, std::move(report),
                   SendResult(SendResult::Status::kDropped,
                              /*http_response_code=*/0));
      continue;
    }

    if (log_metrics)
      LogMetricsOnReportSend(report, now);

    GURL report_url = report.ReportURL();
    std::string report_body = report.ReportBody();
    network_sender_->SendReport(
        std::move(report_url), std::move(report_body),
        base::BindOnce(&AttributionManagerImpl::OnReportSent,
                       weak_factory_.GetWeakPtr(), done, std::move(report)));
  }
}

void AttributionManagerImpl::MarkReportCompleted(
    AttributionReport::EventLevelData::Id report_id) {
  size_t num_removed = reports_being_sent_.erase(report_id);
  DCHECK_EQ(num_removed, 1u);
}

void AttributionManagerImpl::OnReportSent(base::OnceClosure done,
                                          AttributionReport report,
                                          SendResult info) {
  DCHECK(report.ReportId().has_value());

  // If there was a transient failure, and another attempt is allowed,
  // update the report's DB state to reflect that. Otherwise, delete the report
  // from storage if it wasn't skipped due to the browser being offline.

  bool should_retry = false;
  if (info.status == SendResult::Status::kTransientFailure) {
    report.set_failed_send_attempts(report.failed_send_attempts() + 1);
    const absl::optional<base::TimeDelta> delay =
        attribution_policy_->GetFailedReportDelay(
            report.failed_send_attempts());
    if (delay.has_value()) {
      should_retry = true;
      report.set_report_time(report.report_time() + *delay);
    }
  }

  DCHECK(absl::holds_alternative<AttributionReport::EventLevelData::Id>(
      *report.ReportId()));
  const auto report_id =
      absl::get<AttributionReport::EventLevelData::Id>(*report.ReportId());

  if (should_retry) {
    // After updating the report's failure count and new report time in the DB,
    // add it directly to the queue so that the retry is attempted as the new
    // report time is reached, rather than wait for the next DB-polling to
    // occur.
    attribution_storage_
        .AsyncCall(&AttributionStorage::UpdateReportForSendFailure)
        .WithArgs(report_id, report.report_time())
        .Then(base::BindOnce(
            [](base::OnceClosure done,
               base::WeakPtr<AttributionManagerImpl> manager,
               AttributionReport::EventLevelData::Id report_id,
               base::Time new_report_time, bool success) {
              std::move(done).Run();

              if (manager && success) {
                manager->MarkReportCompleted(report_id);
                manager->UpdateGetReportsToSendTimer(new_report_time);
                manager->NotifyReportsChanged();
              }
            },
            std::move(done), weak_factory_.GetWeakPtr(), report_id,
            report.report_time()));
  } else {
    RecordDeleteEvent(DeleteEvent::kStarted);
    attribution_storage_.AsyncCall(&AttributionStorage::DeleteReport)
        .WithArgs(report_id)
        .Then(base::BindOnce(
            [](base::OnceClosure done,
               base::WeakPtr<AttributionManagerImpl> manager,
               AttributionReport::EventLevelData::Id report_id, bool success) {
              std::move(done).Run();
              RecordDeleteEvent(success ? DeleteEvent::kSucceeded
                                        : DeleteEvent::kFailed);

              if (manager && success) {
                manager->MarkReportCompleted(report_id);
                manager->NotifyReportsChanged();
              }
            },
            std::move(done), weak_factory_.GetWeakPtr(), report_id));

    base::UmaHistogramEnumeration(
        "Conversions.ReportSendOutcome",
        ConvertToConversionReportSendOutcome(info.status));
  }

  // TODO(apaseltiner): Consider surfacing retry attempts in internals UI.
  if (info.status != SendResult::Status::kSent &&
      info.status != SendResult::Status::kFailure &&
      info.status != SendResult::Status::kDropped) {
    return;
  }

  for (Observer& observer : observers_)
    observer.OnReportSent(report, info);
}

void AttributionManagerImpl::NotifySourcesChanged() {
  for (Observer& observer : observers_)
    observer.OnSourcesChanged();
}

void AttributionManagerImpl::NotifyReportsChanged() {
  for (Observer& observer : observers_)
    observer.OnReportsChanged();
}

void AttributionManagerImpl::NotifySourceDeactivated(
    const AttributionStorage::DeactivatedSource& source) {
  for (Observer& observer : observers_)
    observer.OnSourceDeactivated(source);
}

}  // namespace content
