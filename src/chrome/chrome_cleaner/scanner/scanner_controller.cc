// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/scanner/scanner_controller.h"

#include <stdlib.h>
#include <time.h>
#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/os/process.h"
#include "chrome/chrome_cleaner/os/shutdown_watchdog.h"
#include "chrome/chrome_cleaner/settings/settings.h"

namespace chrome_cleaner {

namespace {

// The maximal allowed time to run the scanner (5 minutes).
const uint32_t kWatchdogTimeoutInSeconds = 5 * 60;

ResultCode GetResultCodeFromFoundUws(const std::vector<UwSId>& found_uws) {
  for (UwSId uws_id : found_uws) {
    if (!PUPData::IsKnownPUP(uws_id))
      return RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS;
  }

  // Removal has precedence over other states.
  if (PUPData::HasFlaggedPUP(found_uws, &PUPData::HasRemovalFlag)) {
    return RESULT_CODE_SUCCESS;
  }

  if (PUPData::HasFlaggedPUP(found_uws, &PUPData::HasConfirmedUwSFlag)) {
    return RESULT_CODE_EXAMINED_FOR_REMOVAL_ONLY;
  }

  if (PUPData::HasFlaggedPUP(found_uws, &PUPData::HasReportOnlyFlag)) {
    return RESULT_CODE_REPORT_ONLY_PUPS_FOUND;
  }

  DCHECK(found_uws.empty());
  return RESULT_CODE_NO_PUPS_FOUND;
}

}  // namespace

ScannerController::~ScannerController() = default;

int ScannerController::ScanOnly() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure the scanning process gets completed in a reasonable amount of
  // time, otherwise log it and terminate the process.
  base::TimeDelta watchdog_timeout =
      base::TimeDelta::FromSeconds(watchdog_timeout_in_seconds_);

  Settings* settings = Settings::GetInstance();
  if (settings->scanning_timeout_overridden())
    watchdog_timeout = settings->scanning_timeout();

  std::unique_ptr<ShutdownWatchdog> watchdog;
  if (!watchdog_timeout.is_zero()) {
    watchdog = std::make_unique<ShutdownWatchdog>(
        watchdog_timeout,
        base::BindOnce(&ScannerController::WatchdogTimeoutCallback,
                       base::Unretained(this)));
    watchdog->Arm();
  }

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitWhenIdleClosure();
  StartScan();
  run_loop.Run();

  if (watchdog)
    watchdog->Disarm();

  DCHECK_NE(RESULT_CODE_INVALID, result_code_);
  return static_cast<int>(result_code_);
}

ScannerController::ScannerController(RegistryLogger* registry_logger)
    : registry_logger_(registry_logger),
      watchdog_timeout_in_seconds_(kWatchdogTimeoutInSeconds) {
  DCHECK(registry_logger);
}

void ScannerController::DoneScanning(ResultCode status,
                                     const std::vector<UwSId>& found_pups) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateScanResults(found_pups);
  if (status == RESULT_CODE_SUCCESS)
    status = GetResultCodeFromFoundUws(found_pups);
  {
    base::AutoLock lock(lock_);
    result_code_ = status;
  }

  LoggingServiceAPI* logging_service_api = LoggingServiceAPI::GetInstance();

  SystemResourceUsage stats;
  if (GetSystemResourceUsage(::GetCurrentProcess(), &stats))
    logging_service_api->LogProcessInformation(SandboxType::kNonSandboxed,
                                               stats);
  std::map<SandboxType, SystemResourceUsage> sbox_process_usage =
      GetSandboxSystemResourceUsage();
  for (const auto& type_usage : sbox_process_usage) {
    LoggingServiceAPI::GetInstance()->LogProcessInformation(type_usage.first,
                                                            type_usage.second);
  }

  logging_service_api->SetExitCode(status);
  logging_service_api->MaybeSaveLogsToFile(L"");
  logging_service_api->SendLogsToSafeBrowsing(
      base::BindRepeating(&ScannerController::LogsUploadComplete,
                          base::Unretained(this)),
      registry_logger_);
}

void ScannerController::UpdateScanResults(
    const std::vector<UwSId>& found_pups) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Log which PUPs were found.
  registry_logger_->RecordFoundPUPs(found_pups);

  ResultCode result = GetResultCodeFromFoundUws(found_pups);
  {
    base::AutoLock lock(lock_);
    result_code_ = result;
  }
}

int ScannerController::WatchdogTimeoutCallback() {
  ResultCode result_code;
  {
    base::AutoLock lock(lock_);
    result_code = result_code_;
  }

  int watchdog_result_code =
      result_code == RESULT_CODE_SUCCESS
          ? RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS
          : RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS;

  registry_logger_->WriteExitCode(watchdog_result_code);
  registry_logger_->WriteEndTime();

  return watchdog_result_code;
}

void ScannerController::LogsUploadComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                std::move(quit_closure_));
}

}  // namespace chrome_cleaner
