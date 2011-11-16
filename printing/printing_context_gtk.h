// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_GTK_H_
#define PRINTING_PRINTING_CONTEXT_GTK_H_

#include <string>

#include "printing/printing_context.h"

namespace base {
class DictionaryValue;
}

namespace printing {

class Metafile;
class PrintDialogGtkInterface;

class PRINTING_EXPORT PrintingContextGtk : public PrintingContext {
 public:
  explicit PrintingContextGtk(const std::string& app_locale);
  virtual ~PrintingContextGtk();

  // Sets the function that creates the print dialog.
  static void SetCreatePrintDialogFunction(
      PrintDialogGtkInterface* (*create_dialog_func)(
          PrintingContextGtk* context));

  // Prints the document contained in |metafile|.
  void PrintDocument(const Metafile* metafile);

  // PrintingContext implementation.
  virtual void AskUserForSettings(gfx::NativeView parent_view,
                                  int max_pages,
                                  bool has_selection,
                                  PrintSettingsCallback* callback) OVERRIDE;
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

 private:
  string16 document_name_;
  PrintDialogGtkInterface* print_dialog_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextGtk);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_GTK_H_

