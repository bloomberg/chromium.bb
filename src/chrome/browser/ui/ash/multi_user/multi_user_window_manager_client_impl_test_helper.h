// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_IMPL_TEST_HELPER_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_IMPL_TEST_HELPER_H_

#include "base/macros.h"

class MultiUserWindowManagerClientImplTestHelper {
 public:
  static void FlushBindings();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MultiUserWindowManagerClientImplTestHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_CLIENT_IMPL_TEST_HELPER_H_
