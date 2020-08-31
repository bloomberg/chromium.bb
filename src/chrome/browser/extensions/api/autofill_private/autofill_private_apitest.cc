// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

class AutofillPrivateApiTest : public ExtensionApiTest {
 public:
  AutofillPrivateApiTest() {}
  ~AutofillPrivateApiTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunAutofillSubtest(const std::string& subtest) {
    return RunExtensionSubtest("autofill_private", "main.html?" + subtest,
                               kFlagNone, kFlagLoadAsComponent);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPrivateApiTest);
};

}  // namespace

// TODO(hcarmona): Investigate converting these tests to unittests.

// TODO(crbug.com/643097) Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, DISABLED_GetCountryList) {
  EXPECT_TRUE(RunAutofillSubtest("getCountryList")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, GetAddressComponents) {
  EXPECT_TRUE(RunAutofillSubtest("getAddressComponents")) << message_;
}

// TODO(crbug.com/643097) Disabled for flakiness.
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, DISABLED_RemoveEntry) {
  EXPECT_TRUE(RunAutofillSubtest("removeEntry")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, ValidatePhoneNumbers) {
  EXPECT_TRUE(RunAutofillSubtest("ValidatePhoneNumbers")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddAndUpdateAddress) {
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateAddress")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiTest, AddAndUpdateCreditCard) {
  EXPECT_TRUE(RunAutofillSubtest("addAndUpdateCreditCard")) << message_;
}

}  // namespace extensions
