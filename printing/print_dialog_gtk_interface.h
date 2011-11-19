// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_DIALOG_GTK_INTERFACE_H_
#define PRINTING_PRINT_DIALOG_GTK_INTERFACE_H_

#include "base/string16.h"
#include "printing/printing_context_gtk.h"

namespace printing {

class Metafile;
class PrintSettings;

// An interface for GTK printing dialogs. Classes that live outside of
// printing/ can implement this interface and get threading requirements
// correct without exposing those requirements to printing/.
class PrintDialogGtkInterface {
 public:
  // Tell the dialog to use the default print setting.
  virtual void UseDefaultSettings() = 0;

  // Update the dialog to use |job_settings| and |ranges|, where |job_settings|
  // is a dictionary of settings with possible keys from
  // printing/print_job_constants.h. Only used when printing without the system
  // print dialog. E.g. for Print Preview. Returns false on error.
  virtual bool UpdateSettings(const base::DictionaryValue& job_settings,
                              const PageRanges& ranges,
                              PrintSettings* settings) = 0;

  // Shows the dialog and handles the response with |callback|. Only used when
  // printing with the native print dialog.
  virtual void ShowDialog(
      const PrintingContextGtk::PrintSettingsCallback& callback) = 0;

  // Prints the document named |document_name| contained in |metafile|.
  // Called from the print worker thread. Once called, the
  // PrintDialogGtkInterface instance should not be reused.
  virtual void PrintDocument(const Metafile* metafile,
                             const string16& document_name) = 0;

  // Same as AddRef/Release, but with different names since
  // PrintDialogGtkInterface does not inherit from RefCounted.
  virtual void AddRefToDialog() = 0;
  virtual void ReleaseDialog() = 0;

 protected:
  virtual ~PrintDialogGtkInterface() {}
};

}  // namespace printing

#endif  // PRINTING_PRINT_DIALOG_GTK_INTERFACE_H_
