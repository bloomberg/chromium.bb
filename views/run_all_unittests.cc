// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/app_paths.h"
#include "base/test/test_suite.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

class ViewTestSuite : public base::TestSuite {
 public:
  ViewTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  virtual void Initialize() {
    app::RegisterPathProvider();
    ui::RegisterPathProvider();
    base::TestSuite::Initialize();
    ResourceBundle::InitSharedInstance("en-US");
  }
};

int main(int argc, char **argv) {
  return ViewTestSuite(argc, argv).Run();
}
