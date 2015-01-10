// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/ui_thread_gpu.h"

#include "base/thread_task_runner_handle.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ui/ozone/public/gpu_platform_support.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ui {

namespace {

const int kGpuProcessHostId = 1;

}  // namespace

class FakeGpuProcess : public IPC::Sender {
 public:
  FakeGpuProcess() : weak_factory_(this) {}
  ~FakeGpuProcess() {}

  void Init() {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();
    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupport()
        ->OnChannelEstablished(this);
  }

  bool Send(IPC::Message* msg) override {
    DCHECK(task_runner_->BelongsToCurrentThread());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FakeGpuProcess::DispatchToGpuPlatformSupportHostTask,
                   weak_factory_.GetWeakPtr(), msg));
    return true;
  }

 private:
  void DispatchToGpuPlatformSupportHostTask(IPC::Message* msg) {
    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupportHost()
        ->OnMessageReceived(*msg);
    delete msg;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<FakeGpuProcess> weak_factory_;
};

class FakeGpuProcessHost {
 public:
  FakeGpuProcessHost() : weak_factory_(this) {}
  ~FakeGpuProcessHost() {}

  void Init() {
    task_runner_ = base::ThreadTaskRunnerHandle::Get();

    base::Callback<void(IPC::Message*)> sender =
        base::Bind(&FakeGpuProcessHost::DispatchToGpuPlatformSupportTask,
                   weak_factory_.GetWeakPtr());

    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupportHost()
        ->OnChannelEstablished(kGpuProcessHostId, task_runner_, sender);
  }

 private:
  void DispatchToGpuPlatformSupportTask(IPC::Message* msg) {
    DCHECK(task_runner_->BelongsToCurrentThread());
    ui::OzonePlatform::GetInstance()
        ->GetGpuPlatformSupport()
        ->OnMessageReceived(*msg);
    delete msg;
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<FakeGpuProcessHost> weak_factory_;
};

UiThreadGpu::UiThreadGpu() {
}

UiThreadGpu::~UiThreadGpu() {
}

bool UiThreadGpu::Initialize() {
  fake_gpu_process_.reset(new FakeGpuProcess);
  fake_gpu_process_->Init();

  fake_gpu_process_host_.reset(new FakeGpuProcessHost);
  fake_gpu_process_host_->Init();

  return true;
}

}  // namespace ui
