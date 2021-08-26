// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/local_printer_handler_chromeos.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace printing {

namespace {
// Used as a callback to `StartGetPrinters()` in tests.
// Increases `call_count` and records values returned by `StartGetPrinters()`.
// TODO(crbug.com/1171579) Get rid of use of base::ListValue.
void RecordPrinterList(size_t& call_count,
                       std::unique_ptr<base::ListValue>& printers_out,
                       const base::ListValue& printers) {
  ++call_count;
  printers_out = printers.CreateDeepCopy();
}

// Used as a callback to `StartGetPrinters` in tests.
// Records that the test is done.
void RecordPrintersDone(bool& is_done_out) {
  is_done_out = true;
}

void RecordGetCapability(base::Value& capabilities_out,
                         base::Value capability) {
  capabilities_out = std::move(capability);
}

void RecordGetEulaUrl(std::string& fetched_eula_url,
                      const std::string& eula_url) {
  fetched_eula_url = eula_url;
}

}  // namespace

// Test that the printer handler runs callbacks with reasonable defaults when
// the mojo connection to ash cannot be established, which should never occur in
// production but may occur in unit/browser tests.
class LocalPrinterHandlerChromeosTest : public testing::Test {
 public:
  LocalPrinterHandlerChromeosTest() = default;
  LocalPrinterHandlerChromeosTest(const LocalPrinterHandlerChromeosTest&) =
      delete;
  LocalPrinterHandlerChromeosTest& operator=(
      const LocalPrinterHandlerChromeos&) = delete;
  ~LocalPrinterHandlerChromeosTest() override = default;

  void SetUp() override {
    local_printer_handler_ = LocalPrinterHandlerChromeos::CreateForTesting();
  }

  LocalPrinterHandlerChromeos* local_printer_handler() {
    return local_printer_handler_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<LocalPrinterHandlerChromeos> local_printer_handler_;
};

TEST_F(LocalPrinterHandlerChromeosTest,
       PrinterStatusRequestNoAsh_ProvidesDefaultValue) {
  base::Value printer_status("unset");
  local_printer_handler()->StartPrinterStatusRequest(
      "printer1",
      base::BindOnce(base::BindLambdaForTesting([&](const base::Value& status) {
        printer_status = status.Clone();
      })));
  EXPECT_EQ(base::Value(), printer_status);
}

TEST_F(LocalPrinterHandlerChromeosTest, GetPrintersNoAsh_ProvidesDefaultValue) {
  size_t call_count = 0;
  std::unique_ptr<base::ListValue> printers;
  bool is_done = false;
  local_printer_handler()->StartGetPrinters(
      base::BindRepeating(&RecordPrinterList, std::ref(call_count),
                          std::ref(printers)),
      base::BindOnce(&RecordPrintersDone, std::ref(is_done)));
  // RecordPrinterList is called when printers are discovered. If no printers
  // are discovered, the function is not called.
  EXPECT_EQ(0u, call_count);
  // RecordPrintersDone is called once printer discovery is finished, even if no
  // printers have been discovered.
  EXPECT_TRUE(is_done);
}

TEST_F(LocalPrinterHandlerChromeosTest,
       GetDefaultPrinterNoAsh_ProvidesDefaultValue) {
  std::string default_printer = "unset";
  local_printer_handler()->GetDefaultPrinter(
      base::BindOnce(base::BindLambdaForTesting(
          [&](const std::string& printer) { default_printer = printer; })));
  EXPECT_EQ("", default_printer);
}

TEST_F(LocalPrinterHandlerChromeosTest,
       GetCapabilityNoAsh_ProvidesDefaultValue) {
  base::Value fetched_caps("unset");
  local_printer_handler()->StartGetCapability(
      "printer1", base::BindOnce(&RecordGetCapability, std::ref(fetched_caps)));
  EXPECT_EQ(base::Value(), fetched_caps);
}

TEST_F(LocalPrinterHandlerChromeosTest, GetEulaUrlNoAsh_ProvidesDefaultValue) {
  std::string fetched_eula_url = "unset";
  local_printer_handler()->StartGetEulaUrl(
      "printer1",
      base::BindOnce(&RecordGetEulaUrl, std::ref(fetched_eula_url)));
  EXPECT_EQ("", fetched_eula_url);
}

TEST(LocalPrinterHandlerChromeos, PrinterToValue) {
  crosapi::mojom::LocalDestinationInfo input("device_name", "printer_name",
                                             "printer_description", false);
  const base::Value kExpectedValue = *base::JSONReader::Read(R"({
   "cupsEnterprisePrinter": false,
   "deviceName": "device_name",
   "printerDescription": "printer_description",
   "printerName": "printer_name"
})");
  EXPECT_EQ(kExpectedValue, LocalPrinterHandlerChromeos::PrinterToValue(input));
}

TEST(LocalPrinterHandlerChromeos, PrinterToValue_ConfiguredViaPolicy) {
  crosapi::mojom::LocalDestinationInfo printer("device_name", "printer_name",
                                               "printer_description", true);
  const base::Value kExpectedValue = *base::JSONReader::Read(R"({
   "cupsEnterprisePrinter": true,
   "deviceName": "device_name",
   "printerDescription": "printer_description",
   "printerName": "printer_name"
})");
  EXPECT_EQ(kExpectedValue,
            LocalPrinterHandlerChromeos::PrinterToValue(printer));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New(
      "device_name", "printer_name", "printer_description", false);

  // TODO(b/195001379, jkopanski): This block of code should be removed once
  // Ash Chrome M94 is on stable channel. Also remove associated "policies"
  // field in kExpectedValue below.
  caps->allowed_color_modes_deprecated = 1;
  caps->allowed_duplex_modes_deprecated = 2;
  caps->allowed_pin_modes_deprecated_version_1 =
      printing::mojom::PinModeRestriction::kPin;
  caps->default_color_mode_deprecated =
      printing::mojom::ColorModeRestriction::kColor;
  caps->default_duplex_mode_deprecated =
      printing::mojom::DuplexModeRestriction::kSimplex;
  caps->default_pin_mode_deprecated =
      printing::mojom::PinModeRestriction::kNoPin;

  const base::Value kExpectedValue = *base::JSONReader::Read(R"({
   "printer": {
      "cupsEnterprisePrinter": false,
      "deviceName": "device_name",
      "policies": {
         "allowedColorModes": 1,
         "allowedDuplexModes": 2,
         "allowedPinModes": 1,
         "defaultColorMode": 2,
         "defaultDuplexMode": 1,
         "defaultPinMode": 2
      },
      "printerDescription": "printer_description",
      "printerName": "printer_name",
      "printerOptions": {}
   }
})");
  EXPECT_EQ(kExpectedValue,
            LocalPrinterHandlerChromeos::CapabilityToValue(std::move(caps)));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue_ConfiguredViaPolicy) {
  auto caps = crosapi::mojom::CapabilitiesResponse::New();
  caps->basic_info = crosapi::mojom::LocalDestinationInfo::New(
      "device_name", "printer_name", "printer_description", true);

  // TODO(b/195001379, jkopanski): This block of code should be removed once
  // Ash Chrome M94 is on stable channel. Also remove associated "policies"
  // field in kExpectedValue below.
  caps->allowed_color_modes_deprecated = 1;
  caps->allowed_duplex_modes_deprecated = 2;
  caps->allowed_pin_modes_deprecated_version_1 =
      printing::mojom::PinModeRestriction::kPin;
  caps->default_color_mode_deprecated =
      printing::mojom::ColorModeRestriction::kColor;
  caps->default_duplex_mode_deprecated =
      printing::mojom::DuplexModeRestriction::kSimplex;
  caps->default_pin_mode_deprecated =
      printing::mojom::PinModeRestriction::kNoPin;

  const base::Value kExpectedValue = *base::JSONReader::Read(R"({
   "printer": {
      "cupsEnterprisePrinter": true,
      "deviceName": "device_name",
      "policies": {
         "allowedColorModes": 1,
         "allowedDuplexModes": 2,
         "allowedPinModes": 1,
         "defaultColorMode": 2,
         "defaultDuplexMode": 1,
         "defaultPinMode": 2
      },
      "printerDescription": "printer_description",
      "printerName": "printer_name",
      "printerOptions": {}
   }
})");
  EXPECT_EQ(kExpectedValue,
            LocalPrinterHandlerChromeos::CapabilityToValue(std::move(caps)));
}

TEST(LocalPrinterHandlerChromeos, CapabilityToValue_EmptyInput) {
  EXPECT_EQ(base::Value(),
            LocalPrinterHandlerChromeos::CapabilityToValue(nullptr));
}

TEST(LocalPrinterHandlerChromeos, StatusToValue) {
  crosapi::mojom::PrinterStatus status;
  status.printer_id = "printer_id";
  status.timestamp = base::Time::FromDoubleT(1e9);
  status.status_reasons.push_back(crosapi::mojom::StatusReason::New(
      crosapi::mojom::StatusReason::Reason::kOutOfInk,
      crosapi::mojom::StatusReason::Severity::kWarning));
  const base::Value kExpectedValue = *base::JSONReader::Read(R"({
   "printerId": "printer_id",
   "statusReasons": [ {
      "reason": 6,
      "severity": 2
   } ],
   "timestamp": 1e+12
})");
  EXPECT_EQ(kExpectedValue, LocalPrinterHandlerChromeos::StatusToValue(status));
}

}  // namespace printing
