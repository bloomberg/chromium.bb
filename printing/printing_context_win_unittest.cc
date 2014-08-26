// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_context_win.h"

#include "printing/printing_test.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

// This test is automatically disabled if no printer is available.
class PrintingContextTest : public PrintingTest<testing::Test>,
                            public PrintingContext::Delegate {
 public:
  void PrintSettingsCallback(PrintingContext::Result result) {
    result_ = result;
  }

  // PrintingContext::Delegate methods.
  virtual gfx::NativeView GetParentView() OVERRIDE { return NULL; }
  virtual std::string GetAppLocale() OVERRIDE { return std::string(); }

 protected:
  PrintingContext::Result result() const { return result_; }

 private:
  PrintingContext::Result result_;
};

TEST_F(PrintingContextTest, Base) {
  if (IsTestCaseDisabled())
    return;

  PrintSettings settings;
  settings.set_device_name(GetDefaultPrinter());
  // Initialize it.
  scoped_ptr<PrintingContext> context(PrintingContext::Create(this));
  EXPECT_EQ(PrintingContext::OK, context->InitWithSettings(settings));

  // The print may lie to use and may not support world transformation.
  // Verify right now.
  XFORM random_matrix = { 1, 0.1f, 0, 1.5f, 0, 1 };
  EXPECT_TRUE(SetWorldTransform(context->context(), &random_matrix));
  EXPECT_TRUE(ModifyWorldTransform(context->context(), NULL, MWT_IDENTITY));
}

}  // namespace printing
