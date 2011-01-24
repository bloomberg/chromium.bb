// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_H_
#define PRINTING_BACKEND_PRINT_BACKEND_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/ref_counted.h"

class DictionaryValue;

// This is the interface for platform-specific code for a print backend
namespace printing {

struct PrinterBasicInfo {
  PrinterBasicInfo();
  ~PrinterBasicInfo();

  std::string printer_name;
  std::string printer_description;
  int printer_status;
  std::map<std::string, std::string> options;
};

typedef std::vector<PrinterBasicInfo> PrinterList;

struct PrinterCapsAndDefaults {
  std::string printer_capabilities;
  std::string caps_mime_type;
  std::string printer_defaults;
  std::string defaults_mime_type;
};

// PrintBackend class will provide interface for different print backends
// (Windows, CUPS) to implement. User will call CreateInstance() to
// obtain available print backend.
// Please note, that PrintBackend is not platform specific, but rather
// print system specific. For example, CUPS is available on both Linux and Mac,
// but not available on ChromeOS, etc. This design allows us to add more
// functionality on some platforms, while reusing core (CUPS) functions.
class PrintBackend : public base::RefCountedThreadSafe<PrintBackend> {
 public:
  virtual ~PrintBackend();

  // Enumerates the list of installed local and network printers.
  virtual bool EnumeratePrinters(PrinterList* printer_list) = 0;

  // Gets the capabilities and defaults for a specific printer.
  virtual bool GetPrinterCapsAndDefaults(
      const std::string& printer_name,
      PrinterCapsAndDefaults* printer_info) = 0;

  // Returns true if printer_name points to a valid printer.
  virtual bool IsValidPrinter(const std::string& printer_name) = 0;

  // Allocate a print backend. If |print_backend_settings| is NULL, default
  // settings will be used.
  // Return NULL if no print backend available.
  static scoped_refptr<PrintBackend> CreateInstance(
      const DictionaryValue* print_backend_settings);
};

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_H_
