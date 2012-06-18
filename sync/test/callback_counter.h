// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_TEST_CALLBACK_COUNTER_H_
#define SYNC_TEST_CALLBACK_COUNTER_H_
#pragma once

namespace browser_sync {

// Helper class to track how many times a callback is triggered.
class CallbackCounter {
 public:
  CallbackCounter() { Reset(); }
  ~CallbackCounter() {}

  void Reset() { times_called_ = 0; }
  void Callback() { ++times_called_; }
  int times_called() const { return times_called_; }

 private:
  int times_called_;

  DISALLOW_COPY_AND_ASSIGN(CallbackCounter);
};

}  // namespace browser_sync

#endif  // SYNC_TEST_CALLBACK_COUNTER_H_
