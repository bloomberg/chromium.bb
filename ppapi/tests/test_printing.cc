// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_printing.h"

#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Printing);

class TestPrinting_Dev : public pp::Printing_Dev {
public:
  explicit TestPrinting_Dev(pp::Instance* instance) :
      pp::Printing_Dev(instance) {}
  virtual ~TestPrinting_Dev() {}
  virtual uint32_t QuerySupportedPrintOutputFormats() { return 0; }
  virtual int32_t PrintBegin(
      const PP_PrintSettings_Dev& print_settings) { return 0; }
  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
      uint32_t page_range_count) {
    return pp::Resource();
  }
  virtual void PrintEnd() {}
  virtual bool IsPrintScalingDisabled() { return false; }
};

TestPrinting::TestPrinting(TestingInstance* instance)
    : TestCase(instance) {
}

void TestPrinting::RunTests(const std::string& filter) {
  RUN_TEST(GetDefaultPrintSettings, filter);
}

std::string TestPrinting::TestGetDefaultPrintSettings() {
  PP_PrintSettings_Dev print_settings;
  bool success =
      TestPrinting_Dev(instance_).GetDefaultPrintSettings(&print_settings);
  ASSERT_TRUE(success);
  // We can't check any margin values because we don't know what they will be,
  // but the following are currently hard-coded.
  ASSERT_EQ(print_settings.orientation, PP_PRINTORIENTATION_NORMAL);
  ASSERT_EQ(
      print_settings.print_scaling_option, PP_PRINTSCALINGOPTION_SOURCE_SIZE);
  ASSERT_EQ(print_settings.grayscale, PP_FALSE);
  ASSERT_EQ(print_settings.format, PP_PRINTOUTPUTFORMAT_PDF);

  PASS();
}
