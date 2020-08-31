// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/unittest_test_suite.h"

namespace weblayer {

class WebLayerTestSuite : public content::ContentTestSuiteBase {
 public:
  WebLayerTestSuite(int argc, char** argv) : ContentTestSuiteBase(argc, argv) {}
  ~WebLayerTestSuite() override = default;

  WebLayerTestSuite(const WebLayerTestSuite&) = delete;
  WebLayerTestSuite& operator=(const WebLayerTestSuite&) = delete;
};

}  // namespace weblayer

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(
      new weblayer::WebLayerTestSuite(argc, argv));
  return base::LaunchUnitTests(argc, argv,
                               base::BindOnce(&content::UnitTestTestSuite::Run,
                                              base::Unretained(&test_suite)));
}
