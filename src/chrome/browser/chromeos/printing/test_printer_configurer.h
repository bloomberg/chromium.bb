// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_TEST_PRINTER_CONFIGURER_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_TEST_PRINTER_CONFIGURER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"

namespace chromeos {

class Printer;

// Test PrinterConfigurer which allows printers to be marked as configured for
// unit tests.
// Does not configure printers. Can be used to verify that configuration was
// initiated by the class under test.
class TestPrinterConfigurer : public PrinterConfigurer {
 public:
  TestPrinterConfigurer();
  ~TestPrinterConfigurer() override;

  // PrinterConfigurer:
  void SetUpPrinter(const Printer& printer,
                    PrinterSetupCallback callback) override;

  // Returns true if the printer with given |printer_id| was set up or
  // explicitly marked as configured before.
  bool IsConfigured(const std::string& printer_id) const;

  void MarkConfigured(const std::string& printer_id);

 private:
  base::flat_set<std::string> configured_printers_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_TEST_PRINTER_CONFIGURER_H_
