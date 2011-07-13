// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend.h"

#include "base/logging.h"

namespace printing {

// Provides a stubbed out PrintBackend implementation for use on ChromeOS.
class PrintBackendChromeOS : public PrintBackend {
 public:
  PrintBackendChromeOS();
  virtual ~PrintBackendChromeOS() {}

  // PrintBackend implementation.
  virtual bool EnumeratePrinters(PrinterList* printer_list);

  virtual std::string GetDefaultPrinterName();

  virtual bool GetPrinterCapsAndDefaults(const std::string& printer_name,
                                         PrinterCapsAndDefaults* printer_info);

  virtual bool IsValidPrinter(const std::string& printer_name);

 private:
};

PrintBackendChromeOS::PrintBackendChromeOS() {}

bool PrintBackendChromeOS::EnumeratePrinters(PrinterList* printer_list) {
  return true;
}



bool PrintBackendChromeOS::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  NOTREACHED();
  return false;
}

std::string PrintBackendChromeOS::GetDefaultPrinterName() {
  return std::string();
}

bool PrintBackendChromeOS::IsValidPrinter(const std::string& printer_name) {
  NOTREACHED();
  return true;
}

scoped_refptr<PrintBackend> PrintBackend::CreateInstance(
    const base::DictionaryValue* print_backend_settings) {
  return new PrintBackendChromeOS();
}

}  // namespace printing

