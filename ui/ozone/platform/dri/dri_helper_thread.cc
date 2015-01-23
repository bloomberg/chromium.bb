// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_helper_thread.h"

namespace ui {

DriHelperThread::DriHelperThread() : Thread("DriHelperThread") {
}

DriHelperThread::~DriHelperThread() {
  Stop();
}

void DriHelperThread::Initialize() {
  DCHECK(!IsRunning());

  if (!StartWithOptions(base::Thread::Options(base::MessageLoop::TYPE_IO, 0)))
    LOG(FATAL) << "Failed to start the IO helper thread";
}

}  // namespace ui
