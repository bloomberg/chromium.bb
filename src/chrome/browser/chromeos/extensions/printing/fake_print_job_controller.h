// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/extensions/printing/print_job_controller.h"

namespace chromeos {
class CupsPrintersManager;
class TestCupsPrintJobManager;
}  // namespace chromeos

namespace extensions {

// Fake print job controller which doesn't send print jobs to actual printing
// pipeline.
// It's used in unit and API integration tests.
class FakePrintJobController : public PrintJobController {
 public:
  FakePrintJobController(chromeos::TestCupsPrintJobManager* print_job_manager,
                         chromeos::CupsPrintersManager* printers_manager);
  ~FakePrintJobController() override;

  // PrintJobController:
  void StartPrintJob(const std::string& extension_id,
                     std::unique_ptr<printing::MetafileSkia> metafile,
                     std::unique_ptr<printing::PrintSettings> settings,
                     StartPrintJobCallback callback) override;
  bool CancelPrintJob(const std::string& job_id) override;
  void OnPrintJobCreated(
      const std::string& extension_id,
      const std::string& job_id,
      base::WeakPtr<chromeos::CupsPrintJob> cups_job) override;
  void OnPrintJobFinished(const std::string& job_id) override;

  // Helper method to be used in tests to access ongoing print jobs.
  chromeos::CupsPrintJob* GetCupsPrintJob(const std::string& job_id);

 private:
  // Not owned by FakePrintJobController.
  chromeos::TestCupsPrintJobManager* print_job_manager_;
  chromeos::CupsPrintersManager* printers_manager_;

  // Stores ongoing print jobs as a mapping from job id to CupsPrintJob.
  base::flat_map<std::string, std::unique_ptr<chromeos::CupsPrintJob>> jobs_;

  // Current job id.
  int job_id_ = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_PRINTING_FAKE_PRINT_JOB_CONTROLLER_H_
