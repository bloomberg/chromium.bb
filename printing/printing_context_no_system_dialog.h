// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
#define PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_

#include <string>

#include "printing/printing_context.h"

namespace base {
class DictionaryValue;
}

namespace printing {

class PRINTING_EXPORT PrintingContextNoSystemDialog : public PrintingContext {
 public:
  explicit PrintingContextNoSystemDialog(const std::string& app_locale);
  virtual ~PrintingContextNoSystemDialog();

  // PrintingContext implementation.
  virtual void AskUserForSettings(gfx::NativeView parent_view,
                                  int max_pages,
                                  bool has_selection,
                                  PrintSettingsCallback* callback);
  virtual Result UseDefaultSettings();
  virtual Result UpdatePrinterSettings(
      const base::DictionaryValue& job_settings,
      const PageRanges& ranges);
  virtual Result InitWithSettings(const PrintSettings& settings);
  virtual Result NewDocument(const string16& document_name);
  virtual Result NewPage();
  virtual Result PageDone();
  virtual Result DocumentDone();
  virtual void Cancel();
  virtual void ReleaseContext();
  virtual gfx::NativeDrawingContext context() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintingContextNoSystemDialog);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
