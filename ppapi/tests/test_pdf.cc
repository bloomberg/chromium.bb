// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_pdf.h"

#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/private/pdf.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(PDF);

TestPDF::TestPDF(TestingInstance* instance)
    : TestCase(instance) {
}

void TestPDF::RunTests(const std::string& filter) {
  RUN_TEST(GetLocalizedString, filter);
  RUN_TEST(GetResourceImage, filter);
}

std::string TestPDF::TestGetLocalizedString() {
  pp::Var string = pp::PDF::GetLocalizedString(instance_,
      PP_RESOURCESTRING_PDFGETPASSWORD);
  ASSERT_TRUE(string.is_string());
  ASSERT_EQ("This document is password protected.  Please enter a password.",
            string.AsString());
  PASS();
}

std::string TestPDF::TestGetResourceImage() {
  pp::ImageData data =
      pp::PDF::GetResourceImage(instance_, PP_RESOURCEIMAGE_PDF_BUTTON_ZOOMIN);
  ASSERT_EQ(43, data.size().width());
  ASSERT_EQ(42, data.size().height());
  for (int i = 0; i < data.size().width(); ++i) {
    for (int j = 0; j < data.size().height(); ++j) {
      pp::Point point(i, j);
      ASSERT_NE(0, *data.GetAddr32(point));
    }
  }
  PASS();
}
