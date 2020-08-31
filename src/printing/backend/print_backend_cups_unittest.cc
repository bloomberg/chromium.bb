// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_cups.h"

#include <cups/cups.h>

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_consts.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

bool IsDestTypeEligible(int dest_type) {
  cups_dest_t* dest = nullptr;
  int num_dests = 0;
  num_dests =
      cupsAddDest(/*name=*/"test_dest", /*instance=*/nullptr, num_dests, &dest);
  if (num_dests != 1)
    return false;

  cups_option_t* options = nullptr;
  int num_options = 0;
  num_options = cupsAddOption(kCUPSOptPrinterType,
                              base::NumberToString(dest_type).c_str(),
                              num_options, &options);
  dest->num_options = num_options;
  dest->options = options;

  PrinterBasicInfo printer_info;
  const bool eligible =
      PrintBackendCUPS::PrinterBasicInfoFromCUPS(*dest, &printer_info);

  cupsFreeDests(num_dests, dest);
  return eligible;
}

}  // namespace

TEST(PrintBackendCupsTest, PrinterBasicInfoFromCUPS) {
  constexpr char kName[] = "printer";
  cups_dest_t* printer = nullptr;
  ASSERT_EQ(
      1, cupsAddDest(kName, /*instance=*/nullptr, /*num_dests=*/0, &printer));

  int num_options = 0;
  cups_option_t* options = nullptr;
#if defined(OS_MACOSX)
  num_options =
      cupsAddOption(kCUPSOptPrinterInfo, "info", num_options, &options);
  num_options = cupsAddOption(kCUPSOptPrinterMakeAndModel, "description",
                              num_options, &options);
#else
  num_options =
      cupsAddOption(kCUPSOptPrinterInfo, "description", num_options, &options);
#endif
  printer->num_options = num_options;
  printer->options = options;

  PrinterBasicInfo printer_info;
  EXPECT_TRUE(
      PrintBackendCUPS::PrinterBasicInfoFromCUPS(*printer, &printer_info));
  cupsFreeDests(/*num_dests=*/1, printer);

  EXPECT_EQ(kName, printer_info.printer_name);
#if defined(OS_MACOSX)
  EXPECT_EQ("info", printer_info.display_name);
#else
  EXPECT_EQ(kName, printer_info.display_name);
#endif
  EXPECT_EQ("description", printer_info.printer_description);
}

TEST(PrintBackendCupsTest, EligibleDestTypes) {
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_FAX));
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_SCANNER));
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_DISCOVERED));
  EXPECT_TRUE(IsDestTypeEligible(CUPS_PRINTER_LOCAL));

  // Try combos. |CUPS_PRINTER_LOCAL| has a value of 0, but keep these test
  // cases in the event that the constant values change in CUPS.
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_LOCAL | CUPS_PRINTER_FAX));
  EXPECT_FALSE(IsDestTypeEligible(CUPS_PRINTER_LOCAL | CUPS_PRINTER_SCANNER));
}

}  // namespace printing
