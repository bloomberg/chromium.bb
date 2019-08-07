// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_CONTEXT_MAC_H_
#define PRINTING_PRINTING_CONTEXT_MAC_H_

#include <ApplicationServices/ApplicationServices.h>
#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "printing/print_job_constants.h"
#include "printing/printing_context.h"

@class NSPrintInfo;

namespace printing {

class PRINTING_EXPORT PrintingContextMac : public PrintingContext {
 public:
  explicit PrintingContextMac(Delegate* delegate);
  ~PrintingContextMac() override;

  // PrintingContext implementation.
  void AskUserForSettings(int max_pages,
                          bool has_selection,
                          bool is_scripted,
                          PrintSettingsCallback callback) override;
  Result UseDefaultSettings() override;
  gfx::Size GetPdfPaperSizeDeviceUnits() override;
  Result UpdatePrinterSettings(bool external_preview,
                               bool show_system_dialog,
                               int page_count) override;
  Result NewDocument(const base::string16& document_name) override;
  Result NewPage() override;
  Result PageDone() override;
  Result DocumentDone() override;
  void Cancel() override;
  void ReleaseContext() override;
  printing::NativeDrawingContext context() const override;

 private:
  // Initializes PrintSettings from |print_info_|. This must be called
  // after changes to |print_info_| in order for the changes to take effect in
  // printing.
  // This function ignores the page range information specified in the print
  // info object and use |settings_.ranges| instead.
  void InitPrintSettingsFromPrintInfo();

  // Returns the set of page ranges constructed from |print_info_|.
  PageRanges GetPageRangesFromPrintInfo();

  // Updates |print_info_| to use the given printer.
  // Returns true if the printer was set.
  bool SetPrinter(const std::string& device_name);

  // Updates |print_info_| page format with paper selected by user. If paper was
  // not selected, default system paper is used.
  // Returns true if the paper was set.
  bool UpdatePageFormatWithPaperInfo();

  // Updates |print_info_| page format with |paper|.
  // Returns true if the paper was set.
  bool UpdatePageFormatWithPaper(PMPaper paper, PMPageFormat page_format);

  // Sets the print job destination type as preview job.
  // Returns true if the print job destination type is set.
  bool SetPrintPreviewJob();

  // Sets |copies| in PMPrintSettings.
  // Returns true if the number of copies is set.
  bool SetCopiesInPrintSettings(int copies);

  // Sets |collate| in PMPrintSettings.
  // Returns true if |collate| is set.
  bool SetCollateInPrintSettings(bool collate);

  // Sets orientation in native print info object.
  // Returns true if the orientation was set.
  bool SetOrientationIsLandscape(bool landscape);

  // Sets duplex mode in PMPrintSettings.
  // Returns true if duplex mode is set.
  bool SetDuplexModeInPrintSettings(DuplexMode mode);

  // Sets output color mode in PMPrintSettings.
  // Returns true if color mode is set.
  bool SetOutputColor(int color_mode);

  // The native print info object.
  base::scoped_nsobject<NSPrintInfo> print_info_;

  // The current page's context; only valid between NewPage and PageDone call
  // pairs.
  CGContext* context_;

  DISALLOW_COPY_AND_ASSIGN(PrintingContextMac);
};

}  // namespace printing

#endif  // PRINTING_PRINTING_CONTEXT_MAC_H_
