// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_TEST_TEST_SUITE_H_
#define UI_GFX_COMPOSITOR_TEST_TEST_SUITE_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_suite.h"

class MessageLoop;

class CompositorTestSuite : public base::TestSuite {
 public:
  CompositorTestSuite(int argc, char** argv);
  virtual ~CompositorTestSuite();

 protected:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

 private:
  scoped_ptr<MessageLoop> message_loop_;
};

#endif  // UI_GFX_COMPOSITOR_TEST_TEST_SUITE_H_
