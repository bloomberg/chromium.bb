// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/platform_test_helper.h"

namespace views {
namespace {

class DefaultPlatformTestHelper : public PlatformTestHelper {
 public:
  DefaultPlatformTestHelper() {}

  ~DefaultPlatformTestHelper() override {}

  bool IsMus() const override { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPlatformTestHelper);
};

}  // namespace

// static
scoped_ptr<PlatformTestHelper> PlatformTestHelper::Create() {
  return make_scoped_ptr(new DefaultPlatformTestHelper);
}

}  // namespace views
