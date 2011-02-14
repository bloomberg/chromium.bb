// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_cairo.h"

#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>
#include <unicode/ulocdata.h>

#include "base/logging.h"
#include "printing/native_metafile.h"
#include "printing/print_settings_initializer_gtk.h"
#include "printing/units.h"

namespace printing {

// static
  PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(new PrintingContextCairo(app_locale));
}

  PrintingContextCairo::PrintingContextCairo(const std::string& app_locale)
      : PrintingContext(app_locale) {
}

PrintingContextCairo::~PrintingContextCairo() {
  ReleaseContext();
}

void PrintingContextCairo::AskUserForSettings(
    gfx::NativeView parent_view,
    int max_pages,
    bool has_selection,
    PrintSettingsCallback* callback) {
  NOTIMPLEMENTED();
  callback->Run(OK);
}

PrintingContext::Result PrintingContextCairo::UseDefaultSettings() {
  DCHECK(!in_print_job_);

  ResetSettings();
#if defined(OS_CHROMEOS)
  // For Chrome OS use default values based on the app locale rather than rely
  // on GTK. Note that relying on the app locale does not work well if it is
  // different from the system locale, e.g. a user using Chinese ChromeOS in the
  // US. Eventually we need to get the defaults from the printer.
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
  printable_area_device_units.SetRect(
      static_cast<int>(NativeMetafile::kLeftMarginInInch * dpi),
      static_cast<int>(NativeMetafile::kTopMarginInInch * dpi),
      width - (NativeMetafile::kLeftMarginInInch +
          NativeMetafile::kRightMarginInInch) * dpi,
      height - (NativeMetafile::kTopMarginInInch +
          NativeMetafile::kBottomMarginInInch) * dpi);

  settings_.set_dpi(dpi);
  settings_.SetPrinterPrintableArea(physical_size_device_units,
                                    printable_area_device_units,
                                    dpi);
#else  // defined(OS_CHROMEOS)
  GtkWidget* dialog = gtk_print_unix_dialog_new(NULL, NULL);
  GtkPrintSettings* settings =
      gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(dialog));
  GtkPageSetup* page_setup =
      gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(dialog));

  PageRanges ranges_vector;  // Nothing to initialize for default settings.
  PrintSettingsInitializerGtk::InitPrintSettings(
          settings, page_setup, ranges_vector, false, &settings_);

  g_object_unref(settings);
  // |page_setup| is owned by dialog, so it does not need to be unref'ed.
  gtk_widget_destroy(dialog);
#endif  // defined(OS_CHROMEOS)

  return OK;
}

PrintingContext::Result PrintingContextCairo::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  NOTIMPLEMENTED();

  return FAILED;
}

PrintingContext::Result PrintingContextCairo::NewDocument(
    const string16& document_name) {
  DCHECK(!in_print_job_);

  NOTIMPLEMENTED();

  return FAILED;
}

PrintingContext::Result PrintingContextCairo::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  NOTIMPLEMENTED();

  return FAILED;
}

PrintingContext::Result PrintingContextCairo::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  NOTIMPLEMENTED();

  return FAILED;
}

PrintingContext::Result PrintingContextCairo::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  NOTIMPLEMENTED();

  ResetSettings();
  return FAILED;
}

void PrintingContextCairo::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;

  NOTIMPLEMENTED();
}

void PrintingContextCairo::ReleaseContext() {
  // Nothing to do yet.
}

gfx::NativeDrawingContext PrintingContextCairo::context() const {
  return NULL;
}

}  // namespace printing
