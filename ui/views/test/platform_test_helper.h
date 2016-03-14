// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_
#define UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace views {

class PlatformTestHelper {
 public:
  PlatformTestHelper() {}
  virtual ~PlatformTestHelper() {}

  static scoped_ptr<PlatformTestHelper> Create();

  virtual bool IsMus() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformTestHelper);
};

}  // namespace views

#endif  // UI_VIEWS_TEST_PLATFORM_TEST_HELPER_H_
