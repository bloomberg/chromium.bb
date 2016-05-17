// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_RUN_ALL_UNITTESTS_H_
#define UI_VIEWS_RUN_ALL_UNITTESTS_H_

#include "base/callback_forward.h"
#include "base/test/test_suite.h"

#if defined(USE_AURA)
#include <memory>
#endif

namespace aura {
class Env;
}

namespace views {

// TODO(jamescook): Rename this to ViewsTestSuite and rename the file to match.
class ViewTestSuite : public base::TestSuite {
 public:
  ViewTestSuite(int argc, char** argv);
  ~ViewTestSuite() override;

  int RunTests();
  int RunTestsSerially();

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
#if defined(USE_AURA)
  std::unique_ptr<aura::Env> env_;
#endif
  int argc_;
  char** argv_;

  DISALLOW_COPY_AND_ASSIGN(ViewTestSuite);
};

}  // namespace

#endif  // UI_VIEWS_RUN_ALL_UNITTESTS_H_
