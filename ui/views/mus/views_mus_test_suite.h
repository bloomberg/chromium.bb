// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_VIEWS_MUS_TEST_SUITE_H_
#define UI_VIEWS_MUS_VIEWS_MUS_TEST_SUITE_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/run_all_unittests.h"

namespace views {

class ShellConnection;

class ViewsMusTestSuite : public ViewTestSuite {
 public:
  ViewsMusTestSuite(int argc, char** argv);
  ~ViewsMusTestSuite() override;

 private:
  // ViewTestSuite:
  void Initialize() override;
  void Shutdown() override;

  std::unique_ptr<ShellConnection> shell_connections_;

  DISALLOW_COPY_AND_ASSIGN(ViewsMusTestSuite);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_VIEWS_MUS_TEST_SUITE_H_
