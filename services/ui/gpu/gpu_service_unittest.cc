// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_service.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace test {

class GpuServiceTest : public testing::Test {
 public:
  GpuServiceTest()
      : io_thread_("TestIOThread"),
        wait_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~GpuServiceTest() override {}

  GpuService* gpu_service() { return gpu_service_.get(); }

  void DestroyService() { gpu_service_ = nullptr; }

  void BlockIOThread() {
    wait_.Reset();
    io_runner()->PostTask(FROM_HERE, base::Bind(&base::WaitableEvent::Wait,
                                                base::Unretained(&wait_)));
  }

  void UnblockIOThread() {
    DCHECK(!wait_.IsSignaled());
    wait_.Signal();
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_runner() {
    return io_thread_.task_runner();
  }

  // testing::Test
  void SetUp() override {
    ASSERT_TRUE(io_thread_.Start());
    gpu_service_ = base::MakeUnique<GpuService>(
        gpu::GPUInfo(), nullptr /* watchdog_thread */, io_thread_.task_runner(),
        gpu::GpuFeatureInfo());
  }

  void TearDown() override {
    DestroyService();
    base::RunLoop runloop;
    runloop.RunUntilIdle();
    io_thread_.Stop();
  }

 private:
  base::MessageLoop message_loop_;
  base::Thread io_thread_;
  std::unique_ptr<GpuService> gpu_service_;
  base::WaitableEvent wait_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceTest);
};

// Tests that GpuService can be destroyed before Bind() succeeds on the IO
// thread.
TEST_F(GpuServiceTest, ServiceDestroyedBeforeBind) {
  // Block the IO thread to make sure that the GpuService is destroyed before
  // the binding happens on the IO thread.
  mojom::GpuServicePtr ptr;
  BlockIOThread();
  gpu_service()->Bind(mojo::MakeRequest(&ptr));
  UnblockIOThread();
  DestroyService();
}

// Tests ghat GpuService can be destroyed after Bind() succeeds on the IO
// thread.
TEST_F(GpuServiceTest, ServiceDestroyedAfterBind) {
  mojom::GpuServicePtr ptr;
  gpu_service()->Bind(mojo::MakeRequest(&ptr));
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  io_runner()->PostTask(FROM_HERE, base::Bind(&base::WaitableEvent::Signal,
                                              base::Unretained(&wait)));
  wait.Wait();
  DestroyService();
}

}  // namespace test
}  // namespace ui
