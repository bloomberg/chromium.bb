// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/services/printing/public/mojom/print_backend_service.mojom.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(ENABLE_OOP_PRINTING)
#error "OOP printing must be enabled"
#endif

namespace printing {

class PrintedDocument;

// Worker thread code. It manages the PrintingContext and offloads all system
// driver interactions to the Print Backend service.  This is the object that
// generates most NOTIFY_PRINT_JOB_EVENT notifications, but they are generated
// through a NotificationTask task to be executed from the right thread, the UI
// thread.  PrintJob always outlives its worker instance.
class PrintJobWorkerOop : public PrintJobWorker {
 public:
  PrintJobWorkerOop(int render_process_id, int render_frame_id);
  PrintJobWorkerOop(const PrintJobWorkerOop&) = delete;
  PrintJobWorkerOop& operator=(const PrintJobWorkerOop&) = delete;
  ~PrintJobWorkerOop() override;

  // `PrintJobWorker` overrides.
  void StartPrinting(PrintedDocument* new_document) override;

 protected:
  // Local callback wrapper for Print Backend Service mojom call.  Virtual to
  // support testing.
  virtual void OnDidStartPrinting(mojom::ResultCode result);

  // `PrintJobWorker` overrides.
  void UpdatePrintSettings(base::Value new_settings,
                           SettingsCallback callback) override;
  void OnFailure() override;

 private:
  // Support to unregister this worker as a printing client.  Applicable any
  // time a print job finishes, is canceled, or needs to be restarted.
  void UnregisterServiceManagerClient();

  // Local callback wrapper for Print Backend Service mojom call.
  void OnDidUpdatePrintSettings(const std::string& device_name,
                                SettingsCallback callback,
                                mojom::PrintSettingsResultPtr print_settings);

  // Mojo support to send messages from UI thread.
  void SendStartPrinting(const std::string& device_name,
                         const std::u16string& document_name);

  // Client ID with the print backend service manager for this print job.
  // Used only from UI thread.
  absl::optional<uint32_t> service_manager_client_id_;

  // The device name used when printing via a service.  Used only from the UI
  // thread.
  std::string device_name_;

  // The processed name of the document being printed.  Used only from the UI
  // thread.
  std::u16string document_name_;

  // The type of target to print to.  Used only from the UI thread.
  mojom::PrintTargetType print_target_type_ =
      mojom::PrintTargetType::kDirectToDevice;

  // Weak pointers have flags that get bound to the thread where they are
  // checked, so it is necessary to use different factories when getting a
  // weak pointer to send to the worker task runner vs. to the UI thread.
  base::WeakPtrFactory<PrintJobWorkerOop> worker_weak_factory_{this};
  base::WeakPtrFactory<PrintJobWorkerOop> ui_weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_OOP_H_
