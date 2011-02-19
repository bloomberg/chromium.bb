// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_CAIRO_H_
#define PRINTING_PRINTING_CONTEXT_CAIRO_H_

#include <string>

#include "printing/printing_context.h"

#if !defined(OS_CHROMEOS)
#include "printing/native_metafile.h"
#endif

namespace printing {

class PrintingContextCairo : public PrintingContext {
 public:
  explicit PrintingContextCairo(const std::string& app_locale);
  ~PrintingContextCairo();

#if !defined(OS_CHROMEOS)
  // Sets the function that creates the print dialog, and the function that
  // prints the document.
  static void SetPrintingFunctions(
      void* (*create_dialog_func)(PrintSettingsCallback* callback,
                                  PrintingContextCairo* context),
      void (*print_document_func)(void* print_dialog,
                                  const NativeMetafile* metafile,
                                  const string16& document_name));

  // Prints the document contained in |metafile|.
  void PrintDocument(const NativeMetafile* metafile);
#endif

  // PrintingContext implementation.
  virtual void AskUserForSettings(gfx::NativeView parent_view,
                                  int max_pages,
                                  bool has_selection,
                                  PrintSettingsCallback* callback);
  virtual Result UseDefaultSettings();
  virtual Result InitWithSettings(const PrintSettings& settings);
  virtual Result NewDocument(const string16& document_name);
  virtual Result NewPage();
  virtual Result PageDone();
  virtual Result DocumentDone();
  virtual void Cancel();
  virtual void ReleaseContext();
  virtual gfx::NativeDrawingContext context() const;

 private:
#if !defined(OS_CHROMEOS)
  string16 document_name_;
  void* print_dialog_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PrintingContextCairo);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_CAIRO_H_
