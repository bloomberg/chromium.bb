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
  explicit PrintingContextNoSystemDialog(Delegate* delegate);
  virtual ~PrintingContextNoSystemDialog();

  // PrintingContext implementation.
  virtual void AskUserForSettings(
      int max_pages,
      bool has_selection,
      const PrintSettingsCallback& callback) override;
  virtual Result UseDefaultSettings() override;
  virtual gfx::Size GetPdfPaperSizeDeviceUnits() override;
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
  DISALLOW_COPY_AND_ASSIGN(PrintingContextNoSystemDialog);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_NO_SYSTEM_DIALOG_H_
