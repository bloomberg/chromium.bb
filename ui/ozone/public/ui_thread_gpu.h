// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_UI_THREAD_GPU_H_
#define UI_OZONE_PUBLIC_UI_THREAD_GPU_H_

#include "base/memory/scoped_ptr.h"
#include "ui/ozone/ozone_export.h"

namespace IPC {
class Sender;
}

namespace ui {

class UiThreadGpuForwardingSender;

// Helper class for applications that do GL on the UI thead.
//
// This sets up message forwarding between the "gpu" and "ui" threads,
// for applications in which they are both the same thread.
class OZONE_EXPORT UiThreadGpu {
 public:
  UiThreadGpu();
  virtual ~UiThreadGpu();

  // Start processing gpu messages.
  bool Initialize();

 private:
  scoped_ptr<UiThreadGpuForwardingSender> ui_sender_;
  scoped_ptr<UiThreadGpuForwardingSender> gpu_sender_;

  DISALLOW_COPY_AND_ASSIGN(UiThreadGpu);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_UI_THREAD_GPU_H_
