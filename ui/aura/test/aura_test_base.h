// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_AURA_TEST_BASE_H_
#define UI_AURA_TEST_AURA_TEST_BASE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/basictypes.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace aura {
namespace test {

class TestStackingClient;

// A base class for aura unit tests.
class AuraTestBase : public testing::Test {
 public:
  AuraTestBase();
  virtual ~AuraTestBase();

  TestStackingClient* GetTestStackingClient();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  // Flushes message loop.
  void RunAllPendingInMessageLoop();

 private:
  MessageLoopForUI message_loop_;
  bool setup_called_;
  bool teardown_called_;

  DISALLOW_COPY_AND_ASSIGN(AuraTestBase);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_AURA_TEST_BASE_H_
