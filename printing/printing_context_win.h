// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_WIN_H_
#define PRINTING_PRINTING_CONTEXT_WIN_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "printing/printing_context.h"
#include "ui/gfx/native_widget_types.h"

namespace printing {

class PrintSettings;

class PRINTING_EXPORT PrintingContextWin : public PrintingContext {
 public:
  explicit PrintingContextWin(Delegate* delegate);
  virtual ~PrintingContextWin();

  // PrintingContext implementation.
  virtual void AskUserForSettings(
      int max_pages,
      bool has_selection,
      const PrintSettingsCallback& callback) OVERRIDE;
  virtual Result UseDefaultSettings() OVERRIDE;
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() OVERRIDE;
  virtual Result UpdatePrinterSettings(bool external_preview,
                                       bool show_system_dialog) OVERRIDE;
  virtual Result InitWithSettings(const PrintSettings& settings) OVERRIDE;
  virtual Result NewDocument(const base::string16& document_name) OVERRIDE;
  virtual Result NewPage() OVERRIDE;
  virtual Result PageDone() OVERRIDE;
  virtual Result DocumentDone() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual void ReleaseContext() OVERRIDE;
  virtual gfx::NativeDrawingContext context() const OVERRIDE;

 protected:
  static HWND GetRootWindow(gfx::NativeView view);

  // Reads the settings from the selected device context. Updates settings_ and
  // its margins.
  virtual Result InitializeSettings(const base::string16& device_name,
                                    DEVMODE* dev_mode);

  HDC contest() const { return context_; }

  void set_context(HDC context) { context_ = context; }

 private:
  virtual scoped_ptr<DEVMODE, base::FreeDeleter> ShowPrintDialog(
      HANDLE printer,
      gfx::NativeView parent_view,
      DEVMODE* dev_mode);

  // Used in response to the user canceling the printing.
  static BOOL CALLBACK AbortProc(HDC hdc, int nCode);

  // The selected printer context.
  HDC context_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextWin);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_WIN_H_
