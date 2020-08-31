// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_cups_ipp.h"

#include <cups/cups.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "printing/backend/cups_connection.h"
#include "printing/backend/cups_ipp_helper.h"
#include "printing/backend/print_backend_consts.h"
#include "printing/units.h"

namespace printing {

PrintBackendCupsIpp::PrintBackendCupsIpp(
    std::unique_ptr<CupsConnection> cups_connection,
    const std::string& locale)
    : PrintBackend(locale), cups_connection_(std::move(cups_connection)) {}

PrintBackendCupsIpp::~PrintBackendCupsIpp() = default;

bool PrintBackendCupsIpp::EnumeratePrinters(PrinterList* printer_list) {
  DCHECK(printer_list);
  printer_list->clear();

  std::vector<CupsPrinter> printers = cups_connection_->GetDests();
  if (printers.empty()) {
    LOG(WARNING) << "CUPS: Error getting printers from CUPS server"
                 << ", server: " << cups_connection_->server_name()
                 << ", error: "
                 << static_cast<int>(cups_connection_->last_error());

    return false;
  }

  for (const auto& printer : printers) {
    PrinterBasicInfo basic_info;
    if (printer.ToPrinterInfo(&basic_info)) {
      printer_list->push_back(basic_info);
    }
  }

  return true;
}

std::string PrintBackendCupsIpp::GetDefaultPrinterName() {
  std::vector<CupsPrinter> printers = cups_connection_->GetDests();
  for (const auto& printer : printers) {
    if (printer.is_default()) {
      return printer.GetName();
    }
  }

  return std::string();
}

bool PrintBackendCupsIpp::GetPrinterBasicInfo(const std::string& printer_name,
                                              PrinterBasicInfo* printer_info) {
  std::unique_ptr<CupsPrinter> printer(
      cups_connection_->GetPrinter(printer_name));
  if (!printer)
    return false;

  DCHECK_EQ(printer_name, printer->GetName());

  return printer->ToPrinterInfo(printer_info);
}

bool PrintBackendCupsIpp::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_info) {
  NOTREACHED();
  return false;
}

bool PrintBackendCupsIpp::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_info) {
  std::unique_ptr<CupsPrinter> printer(
      cups_connection_->GetPrinter(printer_name));
  if (!printer || !printer->EnsureDestInfo())
    return false;

  CapsAndDefaultsFromPrinter(*printer, printer_info);

  return true;
}

std::string PrintBackendCupsIpp::GetPrinterDriverInfo(
    const std::string& printer_name) {
  std::unique_ptr<CupsPrinter> printer(
      cups_connection_->GetPrinter(printer_name));
  if (!printer)
    return std::string();

  DCHECK_EQ(printer_name, printer->GetName());
  return printer->GetMakeAndModel();
}

bool PrintBackendCupsIpp::IsValidPrinter(const std::string& printer_name) {
  return !!cups_connection_->GetPrinter(printer_name);
}

}  // namespace printing
