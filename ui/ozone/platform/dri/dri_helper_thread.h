// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_DRI_HELPER_THREAD_H_
#define UI_OZONE_PLATFORM_DRI_DRI_HELPER_THREAD_H_

#include "base/threading/thread.h"

namespace ui {

class DriHelperThread : public base::Thread {
 public:
  DriHelperThread();
  ~DriHelperThread() override;

  // Call to start the thread. This needs to be called after the GPU entered the
  // sandbox.
  void Initialize();

 private:
  DISALLOW_COPY_AND_ASSIGN(DriHelperThread);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_DRI_HELPER_THREAD_H_
