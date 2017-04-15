// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementations of IPP requests for printer queue information.

#ifndef PRINTING_BACKEND_CUPS_JOBS_H_
#define PRINTING_BACKEND_CUPS_JOBS_H_

#include <cups/cups.h>

#include <string>
#include <vector>

#include "printing/printing_export.h"

namespace printing {

// Represents a print job sent to the queue.
struct PRINTING_EXPORT CupsJob {
  // Corresponds to job-state from RFC2911.
  enum JobState {
    UNKNOWN,
    PENDING,  // waiting to be processed
    HELD,  // the job has not begun printing and will not without intervention
    COMPLETED,
    PROCESSING,  // job is being sent to the printer/printed
    STOPPED,     // job was being processed and has now stopped
    CANCELED,    // either the spooler or a user canclled the job
    ABORTED      // an error occurred causing the printer to give up
  };

  CupsJob();
  CupsJob(const CupsJob& other);
  ~CupsJob();

  // job id
  int id = -1;
  // printer name in CUPS
  std::string printer_id;
  JobState state = UNKNOWN;
  // the last page printed
  int current_pages = -1;
  // detail for the job state
  std::vector<std::string> state_reasons;
  // human readable message explaining the state
  std::string state_message;
  // most recent timestamp where the job entered PROCESSING
  int processing_started = 0;
};

// Represents the status of a printer containing the properties printer-state,
// printer-state-reasons, and printer-state-message.
struct PRINTING_EXPORT PrinterStatus {
  struct PrinterReason {
    // Standardized reasons from RFC2911.
    enum Reason {
      UNKNOWN_REASON,
      NONE,
      MEDIA_NEEDED,
      MEDIA_JAM,
      MOVING_TO_PAUSED,
      PAUSED,
      SHUTDOWN,
      CONNECTING_TO_DEVICE,
      TIMED_OUT,
      STOPPING,
      STOPPED_PARTLY,
      TONER_LOW,
      TONER_EMPTY,
      SPOOL_AREA_FULL,
      COVER_OPEN,
      INTERLOCK_OPEN,
      DOOR_OPEN,
      INPUT_TRAY_MISSING,
      MEDIA_LOW,
      MEDIA_EMPTY,
      OUTPUT_TRAY_MISSING,
      OUTPUT_AREA_ALMOST_FULL,
      OUTPUT_AREA_FULL,
      MARKER_SUPPLY_LOW,
      MARKER_SUPPLY_EMPTY,
      MARKER_WASTE_ALMOST_FULL,
      MARKER_WASTE_FULL,
      FUSER_OVER_TEMP,
      FUSER_UNDER_TEMP,
      OPC_NEAR_EOL,
      OPC_LIFE_OVER,
      DEVELOPER_LOW,
      DEVELOPER_EMPTY,
      INTERPRETER_RESOURCE_UNAVAILABLE
    };

    // Severity of the state-reason.
    enum Severity { UNKNOWN_SEVERITY, REPORT, WARNING, ERROR };

    Reason reason;
    Severity severity;
  };

  PrinterStatus();
  PrinterStatus(const PrinterStatus& other);
  ~PrinterStatus();

  // printer-state
  ipp_pstate_t state;
  // printer-state-reasons
  std::vector<PrinterReason> reasons;
  // printer-state-message
  std::string message;
};

// Specifies classes of jobs.
enum JobCompletionState {
  COMPLETED,  // only completed jobs
  PROCESSING  // only jobs that are being processed
};

// Extracts structured job information from the |response| for |printer_id|.
// Extracted jobs are added to |jobs|.
void ParseJobsResponse(ipp_t* response,
                       const std::string& printer_id,
                       std::vector<CupsJob>* jobs);

// Attempts to extract a PrinterStatus object out of |response|.
void ParsePrinterStatus(ipp_t* response, PrinterStatus* printer_status);

// Attempts to retrieve printer status using connection |http| for |printer_id|.
// Returns true if succcssful and updates the fields in |printer_status| as
// appropriate.  Returns false if the request failed.
bool GetPrinterStatus(http_t* http,
                      const std::string& printer_id,
                      PrinterStatus* printer_status);

// Attempts to retrieve job information using connection |http| for the printer
// named |printer_id|.  Retrieves at most |limit| jobs.  If |completed| then
// completed jobs are retrieved.  Otherwise, jobs that are currently in progress
// are retrieved.  Results are added to |jobs| if the operation was successful.
bool GetCupsJobs(http_t* http,
                 const std::string& printer_id,
                 int limit,
                 JobCompletionState completed,
                 std::vector<CupsJob>* jobs);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_JOBS_H_
