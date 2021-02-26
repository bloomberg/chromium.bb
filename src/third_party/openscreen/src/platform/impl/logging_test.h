// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_LOGGING_TEST_H_
#define PLATFORM_IMPL_LOGGING_TEST_H_

#include <string>
#include <vector>

// These functions should only be called from logging unittests.

namespace openscreen {

// Append logging output to |messages|.  Each log entry will be written as a new
// element including a newline.  Pass nullptr to stop appending output.  Calling
// this does not affect the behavior of SetLogFifoOrDie().  Normally this should
// only be called for tests as it creates an append-only buffer of log messages
// in memory.
void SetLogBufferForTest(std::vector<std::string>* messages);

// Disables the mechanism to abort a process on an assertion failure.
//
// ***DANGEROUS!*** This will effectively disable assertion enforcement and
// should only be called for assertion macro tests.
//
// *break_was_called will be set to true when Break() is called afterwards.
// Calling with nullptr re-enables normal assertion behavior.
void DisableBreakForTest(bool* break_was_called);

}  // namespace openscreen

#endif  // PLATFORM_IMPL_LOGGING_TEST_H_
