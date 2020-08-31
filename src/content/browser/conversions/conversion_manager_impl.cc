// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/conversions/conversion_manager_impl.h"

#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "base/time/default_clock.h"
#include "content/browser/conversions/conversion_reporter_impl.h"
#include "content/browser/conversions/conversion_storage_delegate_impl.h"
#include "content/browser/conversions/conversion_storage_sql.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"

namespace content {

const constexpr base::TimeDelta kConversionManagerQueueReportsInterval =
    base::TimeDelta::FromMinutes(30);

ConversionManager* ConversionManagerProviderImpl::GetManager(
    WebContents* web_contents) const {
  return static_cast<StoragePartitionImpl*>(
             BrowserContext::GetDefaultStoragePartition(
                 web_contents->GetBrowserContext()))
      ->GetConversionManager();
}

// static
std::unique_ptr<ConversionManagerImpl> ConversionManagerImpl::CreateForTesting(
    std::unique_ptr<ConversionReporter> reporter,
    std::unique_ptr<ConversionPolicy> policy,
    const base::Clock* clock,
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> storage_task_runner) {
  return base::WrapUnique<ConversionManagerImpl>(new ConversionManagerImpl(
      std::move(reporter), std::move(policy), clock, user_data_directory,
      std::move(storage_task_runner)));
}

ConversionManagerImpl::ConversionManagerImpl(
    StoragePartition* storage_partition,
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ConversionManagerImpl(
          std::make_unique<ConversionReporterImpl>(
              storage_partition,
              base::DefaultClock::GetInstance()),
          std::make_unique<ConversionPolicy>(
              base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kConversionsDebugMode)),
          base::DefaultClock::GetInstance(),
          user_data_directory,
          std::move(task_runner)) {}

ConversionManagerImpl::ConversionManagerImpl(
    std::unique_ptr<ConversionReporter> reporter,
    std::unique_ptr<ConversionPolicy> policy,
    const base::Clock* clock,
    const base::FilePath& user_data_directory,
    scoped_refptr<base::SequencedTaskRunner> storage_task_runner)
    : debug_mode_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kConversionsDebugMode)),
      storage_task_runner_(std::move(storage_task_runner)),
      clock_(clock),
      reporter_(std::move(reporter)),
      storage_(new ConversionStorageSql(
                   user_data_directory,
                   std::make_unique<ConversionStorageDelegateImpl>(debug_mode_),
                   clock_),
               base::OnTaskRunnerDeleter(storage_task_runner_)),
      conversion_policy_(std::move(policy)),
      weak_factory_(this) {
  // Unretained is safe because any task to delete |storage_| will be posted
  // after this one.
  base::PostTaskAndReplyWithResult(
      storage_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ConversionStorage::Initialize,
                     base::Unretained(storage_.get())),
      base::BindOnce(&ConversionManagerImpl::OnInitCompleted,
                     weak_factory_.GetWeakPtr()));
}

ConversionManagerImpl::~ConversionManagerImpl() = default;

void ConversionManagerImpl::HandleImpression(
    const StorableImpression& impression) {
  if (!storage_)
    return;

  // Add the impression to storage.
  storage_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConversionStorage::StoreImpression,
                                base::Unretained(storage_.get()), impression));
}

void ConversionManagerImpl::HandleConversion(
    const StorableConversion& conversion) {
  if (!storage_)
    return;

  // TODO(https://crbug.com/1043345): Add UMA for the number of conversions we
  // are logging to storage, and the number of new reports logged to storage.
  // Unretained is safe because any task to delete |storage_| will be posted
  // after this one.
  storage_task_runner_.get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          base::IgnoreResult(
              &ConversionStorage::MaybeCreateAndStoreConversionReports),
          base::Unretained(storage_.get()), conversion));

  // If we are running in debug mode, we should also schedule a task to
  // gather and send any new reports.
  if (debug_mode_)
    GetAndQueueReportsForNextInterval();
}

void ConversionManagerImpl::GetActiveImpressionsForWebUI(
    base::OnceCallback<void(std::vector<StorableImpression>)> callback) {
  // Unretained is safe because any task to delete |storage_| will be posted
  // after this one because |storage_| uses base::OnTaskRunnerDeleter.
  base::PostTaskAndReplyWithResult(
      storage_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ConversionStorage::GetActiveImpressions,
                     base::Unretained(storage_.get())),
      std::move(callback));
}

void ConversionManagerImpl::GetReportsForWebUI(
    base::OnceCallback<void(std::vector<ConversionReport>)> callback,
    base::Time max_report_time) {
  GetAndHandleReports(std::move(callback), max_report_time);
}

void ConversionManagerImpl::SendReportsForWebUI(base::OnceClosure done) {
  GetAndHandleReports(
      base::BindOnce(&ConversionManagerImpl::HandleReportsSentFromWebUI,
                     weak_factory_.GetWeakPtr(), std::move(done)),
      base::Time::Max());
}

const ConversionPolicy& ConversionManagerImpl::GetConversionPolicy() const {
  return *conversion_policy_;
}

void ConversionManagerImpl::ClearData(
    base::Time delete_begin,
    base::Time delete_end,
    base::RepeatingCallback<bool(const url::Origin&)> filter,
    base::OnceClosure done) {
  storage_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ConversionStorage::ClearData,
                     base::Unretained(storage_.get()), delete_begin, delete_end,
                     std::move(filter)),
      std::move(done));
}

void ConversionManagerImpl::OnInitCompleted(bool success) {
  if (!success) {
    storage_.reset();
    return;
  }

  // Once the database is loaded, get all reports that may have expired while
  // Chrome was not running and handle these specially.
  GetAndHandleReports(
      base::BindOnce(&ConversionManagerImpl::HandleReportsExpiredAtStartup,
                     weak_factory_.GetWeakPtr()),
      clock_->Now() + kConversionManagerQueueReportsInterval);

  // Start a repeating timer that will fetch reports once every
  // |kConversionManagerQueueReportsInterval| and add them to |reporter_|.
  get_and_queue_reports_timer_.Start(
      FROM_HERE, kConversionManagerQueueReportsInterval, this,
      &ConversionManagerImpl::GetAndQueueReportsForNextInterval);
}

void ConversionManagerImpl::GetAndHandleReports(
    ReportsHandlerFunc handler_function,
    base::Time max_report_time) {
  base::PostTaskAndReplyWithResult(
      storage_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ConversionStorage::GetConversionsToReport,
                     base::Unretained(storage_.get()), max_report_time),
      std::move(handler_function));
}

void ConversionManagerImpl::GetAndQueueReportsForNextInterval() {
  // Get all the reports that will be reported in the next interval and them to
  // the |reporter_|.
  GetAndHandleReports(base::BindOnce(&ConversionManagerImpl::QueueReports,
                                     weak_factory_.GetWeakPtr()),
                      clock_->Now() + kConversionManagerQueueReportsInterval);
}

void ConversionManagerImpl::QueueReports(
    std::vector<ConversionReport> reports) {
  if (!reports.empty()) {
    // |reporter_| is owned by |this|, so base::Unretained() is safe as the
    // reporter and callbacks will be deleted first.
    reporter_->AddReportsToQueue(
        std::move(reports),
        base::BindRepeating(&ConversionManagerImpl::OnReportSent,
                            base::Unretained(this)));
  }
}

void ConversionManagerImpl::HandleReportsExpiredAtStartup(
    std::vector<ConversionReport> reports) {
  // Add delay to all reports that expired while the browser was not running so
  // they are not temporally join-able.
  base::Time current_time = clock_->Now();
  for (ConversionReport& report : reports) {
    if (report.report_time > current_time)
      continue;

    base::Time updated_report_time =
        conversion_policy_->GetReportTimeForExpiredReportAtStartup(
            current_time);

    report.extra_delay = updated_report_time - report.report_time;
    report.report_time = updated_report_time;
  }
  QueueReports(std::move(reports));
}

void ConversionManagerImpl::HandleReportsSentFromWebUI(
    base::OnceClosure done,
    std::vector<ConversionReport> reports) {
  if (reports.empty()) {
    std::move(done).Run();
    return;
  }

  // All reports should be sent immediately.
  for (ConversionReport& report : reports) {
    report.report_time = base::Time::Min();
  }

  // Wraps |done| so that is will run once all of the reports have finished
  // sending.
  base::RepeatingClosure all_reports_sent =
      base::BarrierClosure(reports.size(), std::move(done));

  // |reporter_| is owned by |this|, so base::Unretained() is safe as the
  // reporter and callbacks will be deleted first.
  reporter_->AddReportsToQueue(
      std::move(reports),
      base::BindRepeating(&ConversionManagerImpl::OnReportSentFromWebUI,
                          base::Unretained(this), std::move(all_reports_sent)));
}

void ConversionManagerImpl::OnReportSent(int64_t conversion_id) {
  storage_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ConversionStorage::DeleteConversion),
                     base::Unretained(storage_.get()), conversion_id));
}

void ConversionManagerImpl::OnReportSentFromWebUI(
    base::OnceClosure reports_sent_barrier,
    int64_t conversion_id) {
  // |reports_sent_barrier| is a OnceClosure view of a RepeatingClosure obtained
  // by base::BarrierClosure().
  storage_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&ConversionStorage::DeleteConversion),
                     base::Unretained(storage_.get()), conversion_id),
      std::move(reports_sent_barrier));
}

}  // namespace content
