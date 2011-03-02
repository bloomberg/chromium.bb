// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_TEST_VIEWS_TEST_BASE_H_
#define VIEWS_TEST_VIEWS_TEST_BASE_H_
#pragma once

#include "testing/gtest/include/gtest/gtest.h"

#include "base/message_loop.h"

namespace views {

// A base class for views unit test. It creates a message loop necessary
// to drive UI events and takes care of OLE initialization for windows.
class ViewsTestBase : public testing::Test {
 public:
  ViewsTestBase() {
#if defined(OS_WIN)
    OleInitialize(NULL);
#endif
  }

  virtual ~ViewsTestBase() {
#if defined(OS_WIN)
    OleUninitialize();
#endif
  }

  virtual void TearDown() {
    // Flush the message loop because we have pending release tasks
    // and these tasks if un-executed would upset Valgrind.
    RunPendingMessages();
  }

  void RunPendingMessages() {
    message_loop_.RunAllPending();
  }

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ViewsTestBase);
};

}  // namespace views

#endif  // VIEWS_TEST_VIEWS_TEST_BASE_H_
