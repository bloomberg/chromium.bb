// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/api/printing.h"
#include "chromeos/crosapi/mojom/local_printer.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class ReadOnlySharedMemoryRegion;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}

namespace printing {
namespace mojom {
class PdfFlattener;
}  // namespace mojom
class PrintSettings;
}  // namespace printing

class NativeWindowTracker;

namespace extensions {

class Extension;
class PrintJobController;

// Handles chrome.printing.submitJob() API request including parsing job
// arguments, sending errors and submitting print job to the printer.
class PrintJobSubmitter {
 public:
  // In case of success |job_id| will contain unique job identifier returned
  // by CUPS. In case of failure |job_id| is nullptr.
  // We could use absl::optional but to be consistent with auto-generated API
  // wrappers we use std::unique_ptr.
  using SubmitJobCallback = base::OnceCallback<void(
      absl::optional<api::printing::SubmitJobStatus> status,
      std::unique_ptr<std::string> job_id,
      absl::optional<std::string> error)>;

  PrintJobSubmitter(gfx::NativeWindow native_window,
                    content::BrowserContext* browser_context,
                    PrintJobController* print_job_controller,
                    mojo::Remote<printing::mojom::PdfFlattener>* pdf_flattener,
                    scoped_refptr<const extensions::Extension> extension,
                    api::printing::SubmitJobRequest request,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                    int local_printer_version,
#endif
                    crosapi::mojom::LocalPrinter* local_printer);
  ~PrintJobSubmitter();

  // Only one call to Start() should happen at a time.
  // |callback| is called asynchronously with the success or failure of the
  // process.
  void Start(SubmitJobCallback callback);

  static base::AutoReset<bool> DisablePdfFlatteningForTesting();

  static base::AutoReset<bool> SkipConfirmationDialogForTesting();

 private:
  friend class PrintingAPIHandler;

  bool CheckContentType() const;

  bool CheckPrintTicket();

  void CheckPrinter();

  void CheckCapabilitiesCompatibility(
      crosapi::mojom::CapabilitiesResponsePtr capabilities);

  void ReadDocumentData();

  void OnDocumentDataRead(std::unique_ptr<std::string> data,
                          int64_t total_blob_length);

  void OnPdfFlattenerDisconnected();

  void OnPdfFlattened(base::ReadOnlySharedMemoryRegion flattened_pdf);

  void ShowPrintJobConfirmationDialog(const gfx::Image& extension_icon);

  void OnPrintJobConfirmationDialogClosed(bool accepted);

  void StartPrintJob();

  void OnPrintJobRejected();

  void OnPrintJobSubmitted(std::unique_ptr<std::string> job_id);

  void FireErrorCallback(const std::string& error);

  gfx::NativeWindow native_window_;
  content::BrowserContext* const browser_context_;

  // Tracks whether |native_window_| got destroyed.
  std::unique_ptr<NativeWindowTracker> native_window_tracker_;

  // These objects are owned by PrintingAPIHandler.
  PrintJobController* const print_job_controller_;
  mojo::Remote<printing::mojom::PdfFlattener>* const pdf_flattener_;

  // TODO(crbug.com/996785): Consider tracking extension being unloaded instead
  // of storing scoped_refptr.
  scoped_refptr<const extensions::Extension> extension_;
  api::printing::SubmitJobRequest request_;
  std::unique_ptr<printing::PrintSettings> settings_;
  std::u16string printer_name_;
  base::ReadOnlySharedMemoryMapping flattened_pdf_mapping_;
  // This is cleared after the request is handled (successfully or not).
  SubmitJobCallback callback_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  const int local_printer_version_;
#endif
  crosapi::mojom::LocalPrinter* const local_printer_;
  base::WeakPtrFactory<PrintJobSubmitter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PRINTING_PRINT_JOB_SUBMITTER_H_
