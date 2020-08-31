// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "printing/page_number.h"
#include "printing/print_job_constants.h"
#include "printing/printing_context.h"

namespace content {
class WebContents;
}

namespace printing {

class PrintJob;
class PrintedDocument;
class PrintedPage;

// Worker thread code. It manages the PrintingContext, which can be blocking
// and/or run a message loop. This is the object that generates most
// NOTIFY_PRINT_JOB_EVENT notifications, but they are generated through a
// NotificationTask task to be executed from the right thread, the UI thread.
// PrintJob always outlives its worker instance.
class PrintJobWorker {
 public:
  using SettingsCallback =
      base::OnceCallback<void(std::unique_ptr<PrintSettings>,
                              PrintingContext::Result)>;

  PrintJobWorker(int render_process_id, int render_frame_id);
  virtual ~PrintJobWorker();

  void SetPrintJob(PrintJob* print_job);

  /* The following functions may only be called before calling SetPrintJob(). */

  // Initializes the print settings. If |ask_user_for_settings| is true, a
  // Print... dialog box will be shown to ask the user their preference.
  // |is_scripted| should be true for calls coming straight from window.print().
  // |is_modifiable| implies HTML and not other formats like PDF.
  void GetSettings(bool ask_user_for_settings,
                   int document_page_count,
                   bool has_selection,
                   MarginType margin_type,
                   bool is_scripted,
                   bool is_modifiable,
                   SettingsCallback callback);

  // Set the new print settings from a dictionary value.
  void SetSettings(base::Value new_settings, SettingsCallback callback);

#if defined(OS_CHROMEOS)
  // Set the new print settings from a POD type.
  void SetSettingsFromPOD(std::unique_ptr<printing::PrintSettings> new_settings,
                          SettingsCallback callback);
#endif

  /* The following functions may only be called after calling SetPrintJob(). */

  // Starts the printing loop. Every pages are printed as soon as the data is
  // available. Makes sure the new_document is the right one.
  void StartPrinting(PrintedDocument* new_document);

  // Updates the printed document.
  void OnDocumentChanged(PrintedDocument* new_document);

  // Dequeues waiting pages. Called when PrintJob receives a
  // NOTIFY_PRINTED_DOCUMENT_UPDATED notification. It's time to look again if
  // the next page can be printed.
  void OnNewPage();

  /* The following functions may be called before or after SetPrintJob(). */

  // Cancels the job.
  void Cancel();

  // Returns true if the thread has been started, and not yet stopped.
  bool IsRunning() const;

  // Posts the given task to be run.
  bool PostTask(const base::Location& from_here, base::OnceClosure task);

  // Signals the thread to exit in the near future.
  void StopSoon();

  // Signals the thread to exit and returns once the thread has exited.
  void Stop();

  // Starts the thread.
  bool Start();

  // Returns the WebContents this work corresponds to.
  content::WebContents* GetWebContents();

 protected:
  // Retrieves the context for testing only.
  PrintingContext* printing_context() { return printing_context_.get(); }

 private:
  // The shared NotificationService service can only be accessed from the UI
  // thread, so this class encloses the necessary information to send the
  // notification from the right thread. Most NOTIFY_PRINT_JOB_EVENT
  // notifications are sent this way, except USER_INIT_DONE, USER_INIT_CANCELED
  // and DEFAULT_INIT_DONE. These three are sent through PrintJob::InitDone().
  class NotificationTask;

  // Posts a task to call OnNewPage(). Used to wait for pages/document to be
  // available.
  void PostWaitForPage();

#if defined(OS_WIN)
  // Windows print GDI-specific handling for OnNewPage().
  bool OnNewPageHelperGdi();

  // Renders a page in the printer.
  // This is applicable when using the Windows GDI print API.
  void SpoolPage(PrintedPage* page);
#endif  // defined(OS_WIN)

  // Renders the document to the printer.
  void SpoolJob();

  // Closes the job since spooling is done.
  void OnDocumentDone();

  // Discards the current document, the current page and cancels the printing
  // context.
  void OnFailure();

  // Asks the user for print settings. Must be called on the UI thread.
  // Required on Mac and Linux. Windows can display UI from non-main threads,
  // but sticks with this for consistency.
  void GetSettingsWithUI(int document_page_count,
                         bool has_selection,
                         bool is_scripted,
                         SettingsCallback callback);

  // Called on the UI thread to update the print settings.
  void UpdatePrintSettings(base::Value new_settings, SettingsCallback callback);

#if defined(OS_CHROMEOS)
  // Called on the UI thread to update the print settings.
  void UpdatePrintSettingsFromPOD(
      std::unique_ptr<printing::PrintSettings> new_settings,
      SettingsCallback callback);
#endif

  // Reports settings back to |callback|.
  void GetSettingsDone(SettingsCallback callback,
                       PrintingContext::Result result);

  // Use the default settings. When using GTK+ or Mac, this can still end up
  // displaying a dialog. So this needs to happen from the UI thread on these
  // systems.
  void UseDefaultSettings(SettingsCallback callback);

  // Printing context delegate.
  const std::unique_ptr<PrintingContext::Delegate> printing_context_delegate_;

  // Information about the printer setting.
  const std::unique_ptr<PrintingContext> printing_context_;

  // The printed document. Only has read-only access.
  scoped_refptr<PrintedDocument> document_;

  // The print job owning this worker thread. It is guaranteed to outlive this
  // object and should be set with SetPrintJob().
  PrintJob* print_job_ = nullptr;

  // Current page number to print.
  PageNumber page_number_;

  // Thread to run worker tasks.
  base::Thread thread_;

  // Thread-safe pointer to task runner of the |thread_|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Used to generate a WeakPtr for callbacks.
  base::WeakPtrFactory<PrintJobWorker> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PrintJobWorker);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_WORKER_H_
