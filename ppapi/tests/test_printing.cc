// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_printing.h"

#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/tests/testing_instance.h"

namespace {
  bool g_callback_triggered;
  int32_t g_callback_result;
}  // namespace

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
    : TestCase(instance),
      nested_event_(instance->pp_instance()) {
  callback_factory_.Initialize(this);
}

void TestPrinting::RunTests(const std::string& filter) {
  RUN_TEST(GetDefaultPrintSettings, filter);
}

std::string TestPrinting::TestGetDefaultPrintSettings() {
  g_callback_triggered = false;
  TestPrinting_Dev test_printing(instance_);
  pp::CompletionCallbackWithOutput<PP_PrintSettings_Dev> cb =
      callback_factory_.NewCallbackWithOutput(&TestPrinting::Callback);
  test_printing.GetDefaultPrintSettings(cb);
  nested_event_.Wait();

  ASSERT_EQ(PP_OK, g_callback_result);
  ASSERT_TRUE(g_callback_triggered);

  PASS();
}

void TestPrinting::Callback(int32_t result,
                            PP_PrintSettings_Dev& /* unused */) {
  g_callback_triggered = true;
  g_callback_result = result;
  nested_event_.Signal();
}
