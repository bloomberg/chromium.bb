// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_globals.h"

#include <gtest/gtest.h>

#include "crazy_linker_system_mock.h"

namespace crazy {

TEST(Globals, Get) {
  SystemMock sys;
  ASSERT_TRUE(Globals::Get());
  ASSERT_TRUE(Globals::GetLibraries());
  ASSERT_TRUE(Globals::GetSearchPaths());
  ASSERT_TRUE(Globals::GetRDebug());
}

}  // namespace crazy
