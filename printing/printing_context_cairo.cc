// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_cairo.h"

#include <gtk/gtk.h>
#include <gtk/gtkprintunixdialog.h>

#include "base/logging.h"

namespace printing {

// static
PrintingContext* PrintingContext::Create() {
  return static_cast<PrintingContext*>(new PrintingContextCairo);
}

PrintingContextCairo::PrintingContextCairo() : PrintingContext() {
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

  GtkWidget* dialog = gtk_print_unix_dialog_new(NULL, NULL);
  GtkPrintSettings* settings =
      gtk_print_unix_dialog_get_settings(GTK_PRINT_UNIX_DIALOG(dialog));
  GtkPageSetup* page_setup =
      gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(dialog));

  PageRanges ranges_vector;  // Nothing to initialize for default settings.
  settings_.Init(settings, page_setup, ranges_vector, false);

  g_object_unref(settings);
  // |page_setup| is owned by dialog, so it does not need to be unref'ed.
  gtk_widget_destroy(dialog);

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

void PrintingContextCairo::DismissDialog() {
  NOTIMPLEMENTED();
}

void PrintingContextCairo::ReleaseContext() {
  // Nothing to do yet.
}

gfx::NativeDrawingContext PrintingContextCairo::context() const {
  return NULL;
}

}  // namespace printing
