// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions used by the tests.

#ifndef RLZ_TEST_RLZ_TEST_HELPERS_H
#define RLZ_TEST_RLZ_TEST_HELPERS_H

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX)
#include "base/files/scoped_temp_dir.h"
#endif

#if defined(OS_WIN)
#include "base/test/test_reg_util_win.h"
#endif

class RlzLibTestNoMachineState : public ::testing::Test {
 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

#if defined(OS_POSIX)
  base::ScopedTempDir temp_dir_;
#endif

#if defined(OS_WIN)
  registry_util::RegistryOverrideManager override_manager_;
#endif
};

class RlzLibTestBase : public RlzLibTestNoMachineState {
 protected:
  virtual void SetUp() OVERRIDE;
};

#endif  // RLZ_TEST_RLZ_TEST_HELPERS_H
