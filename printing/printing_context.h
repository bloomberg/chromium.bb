// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_H_
#define PRINTING_PRINTING_CONTEXT_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <ocidl.h>
#include <commdlg.h>
#endif

#include <string>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "printing/print_settings.h"

#if defined(OS_MACOSX)
#include "base/scoped_cftyperef.h"
#ifdef __OBJC__
@class NSPrintInfo;
#else
class NSPrintInfo;
#endif  // __OBJC__
#endif  // OS_MACOSX

namespace printing {

// Describe the user selected printing context for Windows. This includes the
// OS-dependent UI to ask the user about the print settings. This class directly
// talk to the printer and manages the document and pages breaks.
class PrintingContext {
 public:
  // Tri-state result for user behavior-dependent functions.
  enum Result {
    OK,
    CANCEL,
    FAILED,
  };

  PrintingContext();
  ~PrintingContext();

  // Asks the user what printer and format should be used to print. Updates the
  // context with the select device settings.
  Result AskUserForSettings(gfx::NativeWindow window, int max_pages,
                            bool has_selection);

  // Selects the user's default printer and format. Updates the context with the
  // default device settings.
  Result UseDefaultSettings();

  // Initializes with predefined settings.
  Result InitWithSettings(const PrintSettings& settings);

  // Reinitializes the settings to uninitialized for object reuse.
  void ResetSettings();

  // Does platform specific setup of the printer before the printing. Signal the
  // printer that a document is about to be spooled.
  // Warning: This function enters a message loop. That may cause side effects
  // like IPC message processing! Some printers have side-effects on this call
  // like virtual printers that ask the user for the path of the saved document;
  // for example a PDF printer.
  Result NewDocument(const std::wstring& document_name);

  // Starts a new page.
  Result NewPage();

  // Closes the printed page.
  Result PageDone();

  // Closes the printing job. After this call the object is ready to start a new
  // document.
  Result DocumentDone();

  // Cancels printing. Can be used in a multi-threaded context. Takes effect
  // immediately.
  void Cancel();

  // Dismiss the Print... dialog box if shown.
  void DismissDialog();

  gfx::NativeDrawingContext context() {
#if defined(OS_WIN) || defined(OS_MACOSX)
    return context_;
#else
    NOTIMPLEMENTED();
    return NULL;
#endif
  }

  const PrintSettings& settings() const {
    return settings_;
  }

 private:
  // Class that manages the PrintDlgEx() callbacks. This is meant to be a
  // temporary object used during the Print... dialog display.
  class CallbackHandler;

  // Does bookkeeping when an error occurs.
  PrintingContext::Result OnError();

#if defined(OS_WIN)
  // Used in response to the user canceling the printing.
  static BOOL CALLBACK AbortProc(HDC hdc, int nCode);

  // Reads the settings from the selected device context. Updates settings_ and
  // its margins.
  bool InitializeSettings(const DEVMODE& dev_mode,
                          const std::wstring& new_device_name,
                          const PRINTPAGERANGE* ranges,
                          int number_ranges,
                          bool selection_only);

  // Retrieves the printer's default low-level settings. On Windows, context_ is
  // allocated with this call.
  bool GetPrinterSettings(HANDLE printer,
                          const std::wstring& device_name);

  // Allocates the HDC for a specific DEVMODE.
  bool AllocateContext(const std::wstring& printer_name,
                       const DEVMODE* dev_mode);

  // Parses the result of a PRINTDLGEX result.
  Result ParseDialogResultEx(const PRINTDLGEX& dialog_options);
  Result ParseDialogResult(const PRINTDLG& dialog_options);

#elif defined(OS_MACOSX)
  // Read the settings from the given NSPrintInfo (and cache it for later use).
  void ParsePrintInfo(NSPrintInfo* print_info);
#endif

  // On Windows, the selected printer context.
  // On Mac, the current page's context; only valid between NewPage and PageDone
  // call pairs.
  gfx::NativeDrawingContext context_;

#if defined(OS_MACOSX)
  // The native print info object.
  NSPrintInfo* print_info_;
#endif

  // Complete print context settings.
  PrintSettings settings_;

#ifndef NDEBUG
  // Current page number in the print job.
  int page_number_;
#endif

#if defined(OS_WIN)
  // The dialog box for the time it is shown.
  volatile HWND dialog_box_;
#endif

  // The dialog box has been dismissed.
  volatile bool dialog_box_dismissed_;

  // Is a print job being done.
  volatile bool in_print_job_;

  // Did the user cancel the print job.
  volatile bool abort_printing_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContext);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_H_
