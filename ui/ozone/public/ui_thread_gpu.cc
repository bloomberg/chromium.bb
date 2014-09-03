// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ui_thread_gpu.h"

#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

class UiThreadGpuForwardingSender : public IPC::Sender {
 public:
  explicit UiThreadGpuForwardingSender(IPC::Listener* listener)
      : listener_(listener) {}
  virtual ~UiThreadGpuForwardingSender() {}

  // IPC::Sender:
  virtual bool Send(IPC::Message* msg) OVERRIDE {
    listener_->OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  IPC::Listener* listener_;
};

UiThreadGpu::UiThreadGpu() {
}

UiThreadGpu::~UiThreadGpu() {
}

bool UiThreadGpu::Initialize() {
  OzonePlatform* platform = ui::OzonePlatform::GetInstance();

  ui_sender_.reset(
      new UiThreadGpuForwardingSender(platform->GetGpuPlatformSupportHost()));
  gpu_sender_.reset(
      new UiThreadGpuForwardingSender(platform->GetGpuPlatformSupport()));

  const int kHostId = 1;
  platform->GetGpuPlatformSupportHost()->OnChannelEstablished(
      kHostId, gpu_sender_.get());
  platform->GetGpuPlatformSupport()->OnChannelEstablished(ui_sender_.get());

  return true;
}

}  // namespace ui
