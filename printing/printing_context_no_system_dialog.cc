// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_no_system_dialog.h"

#include <unicode/ulocdata.h>

#include "base/logging.h"
#include "base/values.h"
#include "printing/metafile.h"
#include "printing/print_job_constants.h"
#include "printing/units.h"

namespace printing {

// static
PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(
      new PrintingContextNoSystemDialog(app_locale));
}

PrintingContextNoSystemDialog::PrintingContextNoSystemDialog(
    const std::string& app_locale) : PrintingContext(app_locale) {
}

PrintingContextNoSystemDialog::~PrintingContextNoSystemDialog() {
  ReleaseContext();
}

void PrintingContextNoSystemDialog::AskUserForSettings(
    gfx::NativeView parent_view,
    int max_pages,
    bool has_selection,
    const PrintSettingsCallback& callback) {
  // We don't want to bring up a dialog here.  Ever.  Just signal the callback.
  callback.Run(OK);
}

PrintingContext::Result PrintingContextNoSystemDialog::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();
  // TODO(abodenha): Fetch these settings from the OS where possible.  See
  // bug 102583.
  // TODO(sanjeevr): We need a better feedback loop between the cloud print
  // dialog and this code.
  int dpi = 300;
  gfx::Size physical_size_device_units;
  gfx::Rect printable_area_device_units;
  int32_t width = 0;
  int32_t height = 0;
  UErrorCode error = U_ZERO_ERROR;
  ulocdata_getPaperSize(app_locale_.c_str(), &height, &width, &error);
  if (error != U_ZERO_ERROR) {
    // If the call failed, assume a paper size of 8.5 x 11 inches.
    LOG(WARNING) << "ulocdata_getPaperSize failed, using 8.5 x 11, error: "
                 << error;
    width = static_cast<int>(8.5 * dpi);
    height = static_cast<int>(11 * dpi);
  } else {
    // ulocdata_getPaperSize returns the width and height in mm.
    // Convert this to pixels based on the dpi.
    width = static_cast<int>(ConvertUnitDouble(width, 25.4, 1.0) * dpi);
    height = static_cast<int>(ConvertUnitDouble(height, 25.4, 1.0) * dpi);
  }

  physical_size_device_units.SetSize(width, height);

  // Assume full page is printable for now.
  printable_area_device_units.SetRect(0, 0, width, height);

  settings_.set_dpi(dpi);
  settings_.SetPrinterPrintableArea(physical_size_device_units,
                                    printable_area_device_units,
                                    dpi);

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::UpdatePrinterSettings(
    const DictionaryValue& job_settings, const PageRanges& ranges) {
  bool landscape = false;

  if (!job_settings.GetBoolean(kSettingLandscape, &landscape))
    return OnError();

  if (settings_.dpi() == 0)
    UseDefaultSettings();

  settings_.SetOrientation(landscape);
  settings_.ranges = ranges;

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::NewDocument(
    const string16& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextNoSystemDialog::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  ResetSettings();
  return OK;
}

void PrintingContextNoSystemDialog::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
}

void PrintingContextNoSystemDialog::ReleaseContext() {
  // Intentional No-op.
}

gfx::NativeDrawingContext PrintingContextNoSystemDialog::context() const {
  // Intentional No-op.
  return NULL;
}

}  // namespace printing

