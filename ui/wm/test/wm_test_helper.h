// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_TEST_WM_TEST_HELPER_H_
#define UI_WM_TEST_WM_TEST_HELPER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/test/aura_test_helper.h"

namespace aura {
namespace client {
class DefaultActivationClient;
}
}

namespace gfx {
class Size;
}

namespace wm {

// Creates a minimal environment for running the shell. We can't pull in all of
// ash here, but we can create attach several of the same things we'd find in
// the ash parts of the code.
class WMTestHelper : public aura::test::AuraTestHelper {
 public:
  explicit WMTestHelper(const gfx::Size& default_window_size);
  WMTestHelper();
  virtual ~WMTestHelper();

  // Overridden from aura::test::AuraTestHelper:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  scoped_ptr<aura::client::DefaultActivationClient> activation_client_;

  DISALLOW_COPY_AND_ASSIGN(WMTestHelper);
};

}  // namespace wm

#endif  // UI_WM_TEST_WM_TEST_HELPER_H_
