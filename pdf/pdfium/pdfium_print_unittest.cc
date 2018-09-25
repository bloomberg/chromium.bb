// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_print.h"

#include "base/stl_util.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

using testing::ElementsAre;

TEST(PDFiumPrintTest, GetPageNumbersFromPrintPageNumberRange) {
  std::vector<uint32_t> page_numbers;

  {
    PP_PrintPageNumberRange_Dev page_ranges[] = {{0, 2}};
    page_numbers = PDFiumPrint::GetPageNumbersFromPrintPageNumberRange(
        &page_ranges[0], base::size(page_ranges));
    EXPECT_THAT(page_numbers, ElementsAre(0, 1, 2));
  }
  {
    PP_PrintPageNumberRange_Dev page_ranges[] = {{0, 0}, {2, 2}, {4, 5}};
    page_numbers = PDFiumPrint::GetPageNumbersFromPrintPageNumberRange(
        &page_ranges[0], base::size(page_ranges));
    EXPECT_THAT(page_numbers, ElementsAre(0, 2, 4, 5));
  }
}

}  // namespace chrome_pdf
