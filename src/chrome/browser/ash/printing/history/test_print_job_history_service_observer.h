// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_HISTORY_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_HISTORY_SERVICE_OBSERVER_H_

#include "base/callback.h"
#include "chrome/browser/ash/printing/history/print_job_history_service.h"

namespace ash {

// Observer that counts the number of times it has been called.
class TestPrintJobHistoryServiceObserver
    : public PrintJobHistoryService::Observer {
 public:
  TestPrintJobHistoryServiceObserver(
      PrintJobHistoryService* print_job_history_service,
      base::RepeatingClosure run_loop_closure);
  ~TestPrintJobHistoryServiceObserver();

  int num_print_jobs() { return num_print_jobs_; }

 private:
  // PrintJobHistoryService::Observer:
  void OnPrintJobFinished(
      const chromeos::printing::proto::PrintJobInfo& print_job_info) override;

  PrintJobHistoryService* print_job_history_service_;
  base::RepeatingClosure run_loop_closure_;

  // The number of times the observer is called.
  int num_print_jobs_ = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_HISTORY_TEST_PRINT_JOB_HISTORY_SERVICE_OBSERVER_H_
