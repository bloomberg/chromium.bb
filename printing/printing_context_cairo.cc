// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_cairo.h"

#include "base/logging.h"
#include "printing/units.h"

#if defined(OS_CHROMEOS)
#include <unicode/ulocdata.h>

#include "printing/native_metafile.h"
#include "printing/pdf_ps_metafile_cairo.h"
#else
#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>

#include "printing/print_settings_initializer_gtk.h"
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
namespace {
  // Function pointer for creating print dialogs.
  static void* (*create_dialog_func_)(
      printing::PrintingContext::PrintSettingsCallback* callback,
      printing::PrintingContextCairo* context) = NULL;
  // Function pointer for printing documents.
  static void (*print_document_func_)(
      void* print_dialog,
      const printing::NativeMetafile* metafile,
      const string16& document_name) = NULL;
}  // namespace
#endif  // !defined(OS_CHROMEOS)

namespace printing {

// static
  PrintingContext* PrintingContext::Create(const std::string& app_locale) {
  return static_cast<PrintingContext*>(new PrintingContextCairo(app_locale));
}

PrintingContextCairo::PrintingContextCairo(const std::string& app_locale)
#if defined(OS_CHROMEOS)
    : PrintingContext(app_locale) {
#else
    : PrintingContext(app_locale),
      print_dialog_(NULL) {
#endif
}

PrintingContextCairo::~PrintingContextCairo() {
  ReleaseContext();
}

#if !defined(OS_CHROMEOS)
// static
void PrintingContextCairo::SetPrintingFunctions(
    void* (*create_dialog_func)(PrintSettingsCallback* callback,
                                PrintingContextCairo* context),
    void (*print_document_func)(void* print_dialog,
                                const NativeMetafile* metafile,
                                const string16& document_name)) {
  DCHECK(create_dialog_func);
  DCHECK(print_document_func);
  DCHECK(!create_dialog_func_);
  DCHECK(!print_document_func_);
  create_dialog_func_ = create_dialog_func;
  print_document_func_ = print_document_func;
}

void PrintingContextCairo::PrintDocument(const NativeMetafile* metafile) {
  DCHECK(print_dialog_);
  DCHECK(metafile);
  print_document_func_(print_dialog_, metafile, document_name_);
}
#endif  // !defined(OS_CHROMEOS)

void PrintingContextCairo::AskUserForSettings(
    gfx::NativeView parent_view,
    int max_pages,
    bool has_selection,
    PrintSettingsCallback* callback) {
#if defined(OS_CHROMEOS)
  callback->Run(OK);
#else
  print_dialog_ = create_dialog_func_(callback, this);
#endif  // defined(OS_CHROMEOS)
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
      static_cast<int>(PdfPsMetafile::kLeftMarginInInch * dpi),
      static_cast<int>(PdfPsMetafile::kTopMarginInInch * dpi),
      width - (PdfPsMetafile::kLeftMarginInInch +
          PdfPsMetafile::kRightMarginInInch) * dpi,
      height - (PdfPsMetafile::kTopMarginInInch +
          PdfPsMetafile::kBottomMarginInInch) * dpi);

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

PrintingContext::Result PrintingContextCairo::UpdatePrintSettings(
    const PageRanges& ranges) {
  DCHECK(!in_print_job_);

  settings_.ranges = ranges;

  NOTIMPLEMENTED();

  return FAILED;
}

PrintingContext::Result PrintingContextCairo::InitWithSettings(
    const PrintSettings& settings) {
  DCHECK(!in_print_job_);

  settings_ = settings;

  return OK;
}

PrintingContext::Result PrintingContextCairo::NewDocument(
    const string16& document_name) {
  DCHECK(!in_print_job_);
  in_print_job_ = true;

#if !defined(OS_CHROMEOS)
  document_name_ = document_name;
#endif  // !defined(OS_CHROMEOS)

  return OK;
}

PrintingContext::Result PrintingContextCairo::NewPage() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextCairo::PageDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  // Intentional No-op.

  return OK;
}

PrintingContext::Result PrintingContextCairo::DocumentDone() {
  if (abort_printing_)
    return CANCEL;
  DCHECK(in_print_job_);

  ResetSettings();
  return OK;
}

void PrintingContextCairo::Cancel() {
  abort_printing_ = true;
  in_print_job_ = false;
}

void PrintingContextCairo::ReleaseContext() {
  // Intentional No-op.
}

gfx::NativeDrawingContext PrintingContextCairo::context() const {
  // Intentional No-op.
  return NULL;
}

}  // namespace printing
