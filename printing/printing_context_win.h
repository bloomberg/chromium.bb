// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_WIN_H_
#define PRINTING_PRINTING_CONTEXT_WIN_H_

#include <ocidl.h>
#include <commdlg.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "printing/printing_context.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class PRINTING_EXPORT PrintingContextWin : public PrintingContext {
 public:
  explicit PrintingContextWin(const std::string& app_locale);
  ~PrintingContextWin();

  // PrintingContext implementation.
  virtual void AskUserForSettings(
      gfx::NativeView parent_view,
      int max_pages,
      bool has_selection,
      const PrintSettingsCallback& callback) OVERRIDE;
  virtual Result UseDefaultSettings() OVERRIDE;
  virtual Result UpdatePrinterSettings(
      const base::DictionaryValue& job_settings,
      const PageRanges& ranges) OVERRIDE;
  virtual Result InitWithSettings(const PrintSettings& settings) OVERRIDE;
  virtual Result NewDocument(const string16& document_name) OVERRIDE;
  virtual Result NewPage() OVERRIDE;
  virtual Result PageDone() OVERRIDE;
  virtual Result DocumentDone() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void ReleaseContext() OVERRIDE;
  virtual gfx::NativeDrawingContext context() const OVERRIDE;

#if defined(UNIT_TEST) || defined(PRINTING_IMPLEMENTATION)
  // Sets a fake PrintDlgEx function pointer in tests.
  void SetPrintDialog(HRESULT (__stdcall *print_dialog_func)(LPPRINTDLGEX)) {
    print_dialog_func_ = print_dialog_func;
  }
#endif  // defined(UNIT_TEST)

  // Allocates the HDC for a specific DEVMODE.
  static bool AllocateContext(const std::wstring& printer_name,
                              const DEVMODE* dev_mode,
                              gfx::NativeDrawingContext* context);

  // Retrieves the content of a GetPrinter call.
  static void GetPrinterHelper(HANDLE printer, int level,
                               scoped_array<uint8>* buffer);

 private:
  // Class that manages the PrintDlgEx() callbacks. This is meant to be a
  // temporary object used during the Print... dialog display.
  class CallbackHandler;

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

  // Parses the result of a PRINTDLGEX result.
  Result ParseDialogResultEx(const PRINTDLGEX& dialog_options);
  Result ParseDialogResult(const PRINTDLG& dialog_options);

  // The selected printer context.
  HDC context_;

  // The dialog box for the time it is shown.
  volatile HWND dialog_box_;

  // Function pointer that defaults to PrintDlgEx. It can be changed using
  // SetPrintDialog() in tests.
  HRESULT (__stdcall *print_dialog_func_)(LPPRINTDLGEX);

  DISALLOW_COPY_AND_ASSIGN(PrintingContextWin);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_WIN_H_
