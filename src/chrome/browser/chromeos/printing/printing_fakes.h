// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINTING_FAKES_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINTING_FAKES_H_

#include <vector>

#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chromeos/printing/printer_configuration.h"

namespace chromeos {

class FakeCupsPrintersManager : public CupsPrintersManager {
 public:
  FakeCupsPrintersManager() = default;

  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override {
    return {};
  }

  bool IsPrinterInstalled(const Printer& printer) const override {
    return false;
  }

  std::unique_ptr<Printer> GetPrinter(const std::string& id) const override {
    return nullptr;
  }

  void RemoveUnavailablePrinters(
      std::vector<Printer>* printers) const override {}
  void UpdateConfiguredPrinter(const Printer& printer) override {}
  void RemoveConfiguredPrinter(const std::string& printer_id) override {}
  void AddObserver(CupsPrintersManager::Observer* observer) override {}
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {}
  void PrinterInstalled(const Printer& printer, bool is_automatic) override {}
  void RecordSetupAbandoned(const Printer& printer) override {}
};

class FakePrinterConfigurer : public PrinterConfigurer {
 public:
  void SetUpPrinter(const Printer&, PrinterSetupCallback callback);
};

class FakePpdProvider : public PpdProvider {
 public:
  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ResolvePpdReference(const PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {}
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  // Since PpdProvider is a RefCounted object it's destructor must be either
  // protected or private.
  ~FakePpdProvider() override {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINTING_FAKES_H_
