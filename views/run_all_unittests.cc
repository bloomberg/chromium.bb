// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "views/view.h"

class ViewTestSuite : public base::TestSuite {
 public:
  ViewTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  virtual void Initialize() {
    base::TestSuite::Initialize();

    ui::RegisterPathProvider();
    ui::ResourceBundle::InitSharedInstance("en-US");

#if defined(OS_LINUX)
    // Disable GPU browser compositor during unit tests.
    views::View::set_use_acceleration_when_possible(false);
#endif
  }
};

int main(int argc, char** argv) {
  return ViewTestSuite(argc, argv).Run();
}
