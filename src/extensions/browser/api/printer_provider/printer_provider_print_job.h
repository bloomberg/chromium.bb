// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_PRINT_JOB_H_
#define EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_PRINT_JOB_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/values.h"

namespace extensions {

// Struct describing print job that should be forwarded to an extension via
// chrome.printerProvider.onPrintRequested event.
// TODO(tbarzic): This should probably be a class and have some methods.
struct PrinterProviderPrintJob {
  PrinterProviderPrintJob();
  PrinterProviderPrintJob(PrinterProviderPrintJob&& other);
  PrinterProviderPrintJob& operator=(PrinterProviderPrintJob&& other);
  ~PrinterProviderPrintJob();

  // The id of the printer that should handle the print job. The id is
  // formatted as <extension_id>:<printer_id>, where <extension_id> is the
  // id of the extension that manages the printer, and <printer_id> is
  // the the printer's id within the extension (as reported via
  // chrome.printerProvider.onGetPrintersRequested event callback).
  std::string printer_id;

  // The print job title.
  base::string16 job_title;

  // The print job ticket.
  base::Value ticket;

  // Content type of the document that should be printed.
  std::string content_type;

  // The document data that should be printed.
  scoped_refptr<base::RefCountedMemory> document_bytes;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrinterProviderPrintJob);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_PRINTER_PROVIDER_PRINTER_PROVIDER_PRINT_JOB_H_
