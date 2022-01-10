// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_JOB_H_
#define CHROME_BROWSER_PRINTING_PRINT_JOB_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "printing/print_settings.h"

#if defined(OS_CHROMEOS)
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#endif

namespace base {
class Location;
class RefCountedMemory;
}

namespace printing {

class JobEventDetails;
class MetafilePlayer;
class PrintJobWorker;
class PrintedDocument;
#if defined(OS_WIN)
class PrintedPage;
#endif
class PrinterQuery;
class PrintSettings;

// Manages the print work for a specific document. Talks to the printer through
// PrintingContext through PrintJobWorker. Hides access to PrintingContext in a
// worker thread so the caller never blocks. PrintJob will send notifications on
// any state change. While printing, the PrintJobManager instance keeps a
// reference to the job to be sure it is kept alive. All the code in this class
// runs in the UI thread. All virtual functions are virtual only so that
// TestPrintJob can override them in tests.
class PrintJob : public base::RefCountedThreadSafe<PrintJob> {
 public:
  // An observer interface implemented by classes which are interested
  // in `PrintJob` events.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDocDone(int job_id, PrintedDocument* document) {}
    virtual void OnJobDone() {}
    virtual void OnFailed() {}
  };

#if defined(OS_CHROMEOS)
  // An enumeration of components where print jobs can come from. The order of
  // these enums must match that of
  // chrome/browser/chromeos/printing/history/print_job_info.proto.
  using Source = crosapi::mojom::PrintJob::Source;
#endif  // defined(OS_CHROMEOS)

  // Create a empty PrintJob. When initializing with this constructor,
  // post-constructor initialization must be done with Initialize().
  // If PrintJob is created on Chrome OS, call SetSource() to set which
  // component initiated this print job.
  PrintJob();

  PrintJob(const PrintJob&) = delete;
  PrintJob& operator=(const PrintJob&) = delete;

  // Grabs the ownership of the PrintJobWorker from a PrinterQuery along with
  // the print settings. Sets the expected page count of the print job based on
  // the settings.
  virtual void Initialize(std::unique_ptr<PrinterQuery> query,
                          const std::u16string& name,
                          uint32_t page_count);

#if defined(OS_WIN)
  void StartConversionToNativeFormat(
      scoped_refptr<base::RefCountedMemory> print_data,
      const gfx::Size& page_size,
      const gfx::Rect& content_area,
      const gfx::Point& physical_offsets);

  // Overwrites the PDF page mapping to fill in values of -1 for all indices
  // that are not selected. This is needed when the user opens the system
  // dialog from the link in Print Preview on Windows and then sets a selection
  // of pages, because all PDF pages will be converted, but only the user's
  // selected pages should be sent to the printer. See https://crbug.com/823876.
  void ResetPageMapping();

  // Called when |page| is done printing.
  void OnPageDone(PrintedPage* page);
#endif

  // Called when the document is done printing.
  virtual void OnDocDone(int job_id, PrintedDocument* document);

  // Called if the document fails to print.
  virtual void OnFailed();

  // Starts the actual printing. Signals the worker that it should begin to
  // spool as soon as data is available.
  virtual void StartPrinting();

  // Asks for the worker thread to finish its queued tasks and disconnects the
  // delegate object. The PrintJobManager will remove its reference.
  // WARNING: This may have the side-effect of destroying the object if the
  // caller doesn't have a handle to the object. Use PrintJob::is_stopped() to
  // check whether the worker thread has actually stopped.
  virtual void Stop();

  // Cancels printing job and stops the worker thread. Takes effect immediately.
  // The caller must have a reference to the PrintJob before calling Cancel(),
  // since Cancel() calls Stop(). See WARNING above for Stop().
  virtual void Cancel();

  // Synchronously wait for the job to finish. It is mainly useful when the
  // process is about to be shut down and we're waiting for the spooler to eat
  // our data.
  virtual bool FlushJob(base::TimeDelta timeout);

  // Returns true if the print job is pending, i.e. between a StartPrinting()
  // and the end of the spooling.
  bool is_job_pending() const;

  // Access the current printed document. Warning: may be NULL.
  PrintedDocument* document() const;

  // Access stored settings.
  const PrintSettings& settings() const;

#if defined(OS_CHROMEOS)
  // Sets the component which initiated the print job.
  void SetSource(Source source, const std::string& source_id);

  // Returns the source of print job.
  Source source() const;

  // Returns the ID of the source.
  const std::string& source_id() const;
#endif  // defined(OS_CHROMEOS)

  // Posts the given task to be run.
  bool PostTask(const base::Location& from_here, base::OnceClosure task);

  // Adds and removes observers for `PrintJob` events. The order in
  // which notifications are sent to observers is undefined. Observers must be
  // sure to remove the observer before they go away.
  void AddObserver(Observer& observer);
  void RemoveObserver(Observer& observer);

 protected:
  // Refcounted class.
  friend class base::RefCountedThreadSafe<PrintJob>;

  virtual ~PrintJob();

  // The functions below are used for tests only.
  void set_job_pending(bool pending);

  // Updates |document_| to a new instance. Protected so that tests can access
  // it.
  void UpdatePrintedDocument(scoped_refptr<PrintedDocument> new_document);

 private:
#if defined(OS_WIN)
  FRIEND_TEST_ALL_PREFIXES(PrintJobTest, PageRangeMapping);
#endif

  // Clears reference to |document_|.
  void ClearPrintedDocument();

  // Helper method for UpdatePrintedDocument() and ClearPrintedDocument() to
  // sync |document_| updates with |worker_|.
  void SyncPrintedDocumentToWorker();

  // Releases the worker thread by calling Stop(), then broadcasts a JOB_DONE
  // notification.
  void OnDocumentDone();

  // Terminates the worker thread in a very controlled way, to work around any
  // eventual deadlock.
  void ControlledWorkerShutdown();

  void HoldUntilStopIsCalled();

#if defined(OS_WIN)
  virtual void StartPdfToEmfConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Size& page_size,
      const gfx::Rect& content_area);

  virtual void StartPdfToPostScriptConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Rect& content_area,
      const gfx::Point& physical_offsets,
      bool ps_level2);

  virtual void StartPdfToTextConversion(
      scoped_refptr<base::RefCountedMemory> bytes,
      const gfx::Size& page_size);

  void OnPdfConversionStarted(uint32_t page_count);
  void OnPdfPageConverted(uint32_t page_number,
                          float scale_factor,
                          std::unique_ptr<MetafilePlayer> metafile);

  // Helper method to do the work for ResetPageMapping(). Split for unit tests.
  static std::vector<uint32_t> GetFullPageMapping(
      const std::vector<uint32_t>& pages,
      uint32_t total_page_count);
#endif  // defined(OS_WIN)

  base::ObserverList<Observer> observers_;

  // All the UI is done in a worker thread because many Win32 print functions
  // are blocking and enters a message loop without your consent. There is one
  // worker thread per print job.
  std::unique_ptr<PrintJobWorker> worker_;

  // The printed document.
  scoped_refptr<PrintedDocument> document_;

  // Is the worker thread printing.
  bool is_job_pending_ = false;

  // Is Canceling? If so, try to not cause recursion if on FAILED notification,
  // the notified calls Cancel() again.
  bool is_canceling_ = false;

#if defined(OS_WIN)
  class PdfConversionState;
  std::unique_ptr<PdfConversionState> pdf_conversion_state_;
  std::vector<uint32_t> pdf_page_mapping_;
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
  // The component which initiated the print job.
  Source source_;

  // ID of the source.
  // This should be blank if the source is PRINT_PREVIEW or ARC.
  std::string source_id_;
#endif  // defined(OS_CHROMEOS)

  // Holds the quit closure while running a nested RunLoop to flush tasks.
  base::OnceClosure quit_closure_;
};

// Details for a NOTIFY_PRINT_JOB_EVENT notification. The members may be NULL.
class JobEventDetails : public base::RefCountedThreadSafe<JobEventDetails> {
 public:
  // Event type.
  enum Type {
    // A new document started printing.
    NEW_DOC,

    // A document is done printing. The worker thread is still alive. Warning:
    // not a good moment to release the handle to PrintJob.
    DOC_DONE,

    // The worker thread is finished. A good moment to release the handle to
    // PrintJob.
    JOB_DONE,

    // An error occured. Printing is canceled.
    FAILED
  };

  JobEventDetails(Type type, int job_id, PrintedDocument* document);

  JobEventDetails(const JobEventDetails&) = delete;
  JobEventDetails& operator=(const JobEventDetails&) = delete;

  // Getters.
  PrintedDocument* document() const;
  Type type() const {
    return type_;
  }
  int job_id() const { return job_id_; }

 private:
  friend class base::RefCountedThreadSafe<JobEventDetails>;

  ~JobEventDetails();

  scoped_refptr<PrintedDocument> document_;
  const Type type_;
  int job_id_;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_JOB_H_
