// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_LINUX_H_
#define PRINTING_PRINTING_CONTEXT_LINUX_H_

#include <string>

#include "printing/printing_context.h"

namespace base {
class DictionaryValue;
}

namespace printing {

class MetafilePlayer;
class PrintDialogGtkInterface;

// PrintingContext with optional native UI for print dialog and pdf_paper_size.
class PRINTING_EXPORT PrintingContextLinux : public PrintingContext {
 public:
  explicit PrintingContextLinux(Delegate* delegate);
  virtual ~PrintingContextLinux();

  // Sets the function that creates the print dialog.
  static void SetCreatePrintDialogFunction(
      PrintDialogGtkInterface* (*create_dialog_func)(
          PrintingContextLinux* context));

  // Sets the function that returns pdf paper size through the native API.
  static void SetPdfPaperSizeFunction(
      gfx::Size (*get_pdf_paper_size)(PrintingContextLinux* context));

  // Prints the document contained in |metafile|.
  void PrintDocument(const MetafilePlayer& metafile);

  // PrintingContext implementation.
  virtual void AskUserForSettings(
      int max_pages,
      bool has_selection,
      const PrintSettingsCallback& callback) override;
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() override;
  virtual Result UseDefaultSettings() override;
  virtual Result UpdatePrinterSettings(bool external_preview,
                                       bool show_system_dialog) override;
  virtual Result InitWithSettings(const PrintSettings& settings) override;
  virtual Result NewDocument(const base::string16& document_name) override;
  virtual Result NewPage() override;
  virtual Result PageDone() override;
  virtual Result DocumentDone() override;
  virtual void Cancel() override;
  virtual void ReleaseContext() override;
  virtual gfx::NativeDrawingContext context() const override;

 private:
  base::string16 document_name_;
  PrintDialogGtkInterface* print_dialog_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextLinux);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_LINUX_H_
