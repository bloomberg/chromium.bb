// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper functions used by the tests.

#ifndef RLZ_TEST_RLZ_TEST_HELPERS_H
#define RLZ_TEST_RLZ_TEST_HELPERS_H

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
#include "base/files/scoped_temp_dir.h"
#endif
#if defined(OS_CHROMEOS)
#include "base/message_loop.h"
#include "base/threading/thread.h"
#endif

class RlzLibTestNoMachineState : public ::testing::Test {
 protected:
#if defined(OS_CHROMEOS)
  RlzLibTestNoMachineState();
#endif
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;


#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
 base::ScopedTempDir temp_dir_;
#endif
#if defined(OS_CHROMEOS)
  base::Thread pref_store_io_thread_;
  MessageLoop message_loop_;
#endif
};

class RlzLibTestBase : public RlzLibTestNoMachineState {
  virtual void SetUp() OVERRIDE;
};


#endif  // RLZ_TEST_RLZ_TEST_HELPERS_H
