// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor_setup.h"
#include "ui/compositor/test/compositor_test_support.h"
#include "ui/views/view.h"

class ViewTestSuite : public base::TestSuite {
 public:
  ViewTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  virtual void Initialize() {
    base::TestSuite::Initialize();

    ui::RegisterPathProvider();
    ui::ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

    ui::CompositorTestSupport::Initialize();
    ui::SetupTestCompositor();
  }

  virtual void Shutdown() {
    ui::CompositorTestSupport::Terminate();
  }
};

int main(int argc, char** argv) {
  return ViewTestSuite(argc, argv).Run();
}
