// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/test_print_backend.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/location.h"
#include "base/logging.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"

namespace printing {

namespace {

mojom::ResultCode ReportErrorAccessDenied(const base::Location& from_here) {
  DLOG(ERROR) << from_here.ToString() << " failed, access denied";
  return mojom::ResultCode::kAccessDenied;
}

mojom::ResultCode ReportErrorNoData(const base::Location& from_here) {
  DLOG(ERROR) << from_here.ToString() << " failed, no data";
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode ReportErrorNoDevice(const base::Location& from_here) {
  DLOG(ERROR) << from_here.ToString() << " failed, no such device";
  return mojom::ResultCode::kFailed;
}

mojom::ResultCode ReportErrorNotImplemented(const base::Location& from_here) {
  DLOG(ERROR) << from_here.ToString() << " failed, method not implemented";
  return mojom::ResultCode::kFailed;
}

}  // namespace

TestPrintBackend::TestPrintBackend() : PrintBackend(/*locale=*/std::string()) {}

TestPrintBackend::~TestPrintBackend() = default;

mojom::ResultCode TestPrintBackend::EnumeratePrinters(
    PrinterList* printer_list) {
  DCHECK(printer_list->empty());
  if (printer_map_.empty())
    return mojom::ResultCode::kSuccess;

  for (const auto& entry : printer_map_) {
    const std::unique_ptr<PrinterData>& data = entry.second;

    // Can only return basic info for printers which have registered info.
    if (data->info)
      printer_list->emplace_back(*data->info);
  }
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintBackend::GetDefaultPrinterName(
    std::string& default_printer) {
  default_printer = default_printer_name_;
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintBackend::GetPrinterBasicInfo(
    const std::string& printer_name,
    PrinterBasicInfo* printer_info) {
  auto found = printer_map_.find(printer_name);
  if (found == printer_map_.end()) {
    // Matching entry not found.
    return ReportErrorNoDevice(FROM_HERE);
  }

  const std::unique_ptr<PrinterData>& data = found->second;
  if (data->blocked_by_permissions)
    return ReportErrorAccessDenied(FROM_HERE);

  // Basic info might not have been provided.
  if (!data->info)
    return ReportErrorNoData(FROM_HERE);

  *printer_info = *data->info;
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintBackend::GetPrinterSemanticCapsAndDefaults(
    const std::string& printer_name,
    PrinterSemanticCapsAndDefaults* printer_caps) {
  auto found = printer_map_.find(printer_name);
  if (found == printer_map_.end())
    return ReportErrorNoDevice(FROM_HERE);

  const std::unique_ptr<PrinterData>& data = found->second;
  if (data->blocked_by_permissions)
    return ReportErrorAccessDenied(FROM_HERE);

  // Capabilities might not have been provided.
  if (!data->caps)
    return ReportErrorNoData(FROM_HERE);

  *printer_caps = *data->caps;
  return mojom::ResultCode::kSuccess;
}

mojom::ResultCode TestPrintBackend::GetPrinterCapsAndDefaults(
    const std::string& printer_name,
    PrinterCapsAndDefaults* printer_caps) {
  return ReportErrorNotImplemented(FROM_HERE);
}

std::string TestPrintBackend::GetPrinterDriverInfo(
    const std::string& printer_name) {
  // not implemented
  return "";
}

bool TestPrintBackend::IsValidPrinter(const std::string& printer_name) {
  return base::Contains(printer_map_, printer_name);
}

void TestPrintBackend::SetDefaultPrinterName(const std::string& printer_name) {
  if (default_printer_name_ == printer_name)
    return;

  auto found = printer_map_.find(printer_name);
  if (found == printer_map_.end()) {
    DLOG(ERROR) << "Unable to set an unknown printer as the default.  Unknown "
                << "printer name: " << printer_name;
    return;
  }

  // Previous default printer is no longer the default.
  const std::unique_ptr<PrinterData>& new_default_data = found->second;
  if (!default_printer_name_.empty())
    printer_map_[default_printer_name_]->info->is_default = false;

  // Now update new printer as default.
  default_printer_name_ = printer_name;
  if (!default_printer_name_.empty())
    new_default_data->info->is_default = true;
}

void TestPrintBackend::AddValidPrinter(
    const std::string& printer_name,
    std::unique_ptr<PrinterSemanticCapsAndDefaults> caps,
    std::unique_ptr<PrinterBasicInfo> info) {
  AddPrinter(printer_name, std::move(caps), std::move(info),
             /*blocked_by_permissions=*/false);
}

void TestPrintBackend::AddInvalidDataPrinter(const std::string& printer_name) {
  // The blank fields in default `PrinterBasicInfo` cause Mojom data validation
  // errors.
  AddPrinter(printer_name, std::make_unique<PrinterSemanticCapsAndDefaults>(),
             std::make_unique<PrinterBasicInfo>(),
             /*blocked_by_permissions=*/false);
}

void TestPrintBackend::AddAccessDeniedPrinter(const std::string& printer_name) {
  AddPrinter(printer_name, /*caps=*/nullptr, /*info=*/nullptr,
             /*blocked_by_permissions=*/true);
}

void TestPrintBackend::AddPrinter(
    const std::string& printer_name,
    std::unique_ptr<PrinterSemanticCapsAndDefaults> caps,
    std::unique_ptr<PrinterBasicInfo> info,
    bool blocked_by_permissions) {
  DCHECK(!printer_name.empty());

  const bool is_default = info && info->is_default;
  printer_map_[printer_name] = std::make_unique<PrinterData>(
      std::move(caps), std::move(info), blocked_by_permissions);

  // Ensure that default settings are honored if more than one is attempted to
  // be marked as default or if this prior default should no longer be so.
  if (is_default)
    SetDefaultPrinterName(printer_name);
  else if (default_printer_name_ == printer_name)
    default_printer_name_.clear();
}

TestPrintBackend::PrinterData::PrinterData(
    std::unique_ptr<PrinterSemanticCapsAndDefaults> caps,
    std::unique_ptr<PrinterBasicInfo> info,
    bool blocked_by_permissions)
    : caps(std::move(caps)),
      info(std::move(info)),
      blocked_by_permissions(blocked_by_permissions) {}

TestPrintBackend::PrinterData::~PrinterData() = default;

}  // namespace printing
