// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/gpu_host.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/config/gpu_info.h"
#include "services/ui/public/interfaces/gpu.mojom.h"
#include "services/ui/ws/gpu_client.h"
#include "services/ui/ws/gpu_host_delegate.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#undef None
#undef Bool
#endif  // USE_X11

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ws {
namespace test {
namespace {

// No-opt implementation of GpuHostDelegate.
class TestGpuHostDelegate : public GpuHostDelegate {
 public:
  TestGpuHostDelegate() {}
  ~TestGpuHostDelegate() override {}

  // GpuHostDelegate:
  void OnGpuServiceInitialized() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestGpuHostDelegate);
};

// Test implementation of GpuService. For testing behaviour of calls made by
// GpuClient
class TestGpuService : public viz::GpuServiceImpl {
 public:
  explicit TestGpuService(
      scoped_refptr<base::SingleThreadTaskRunner> io_runner);
  ~TestGpuService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestGpuService);
};

TestGpuService::TestGpuService(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner)
    : GpuServiceImpl(gpu::GPUInfo(),
                     nullptr /* watchdog_thread */,
                     std::move(io_runner),
                     gpu::GpuFeatureInfo()) {}

}  // namespace

class GpuHostTest : public testing::Test {
 public:
  GpuHostTest() : io_thread_("IOThread") {
    CHECK(io_thread_.Start());
    gpu_service_ = base::MakeUnique<TestGpuService>(io_thread_.task_runner());
  }
  ~GpuHostTest() override {}

  GpuHost* gpu_host() { return gpu_host_.get(); }

  base::WeakPtr<GpuClient> AddGpuClient();
  void DestroyHost();

  // testing::Test
  void SetUp() override;

 private:
  base::MessageLoop message_loop_;

  base::WeakPtr<GpuClient> client_ref_;

  base::Thread io_thread_;
  TestGpuHostDelegate gpu_host_delegate_;
  std::unique_ptr<TestGpuService> gpu_service_;
  viz::mojom::GpuServicePtr gpu_service_ptr_;
  std::unique_ptr<DefaultGpuHost> gpu_host_;

  DISALLOW_COPY_AND_ASSIGN(GpuHostTest);
};

base::WeakPtr<GpuClient> GpuHostTest::AddGpuClient() {
  GpuClient* client = gpu_host_->AddInternal(mojom::GpuRequest());
  return client->weak_factory_.GetWeakPtr();
}

void GpuHostTest::DestroyHost() {
  gpu_host_.reset();
}

void GpuHostTest::SetUp() {
  testing::Test::SetUp();
  gpu_host_ = base::MakeUnique<DefaultGpuHost>(&gpu_host_delegate_);
  gpu_service_->Bind(mojo::MakeRequest(&gpu_service_ptr_));
  gpu_host_->gpu_service_ = std::move(gpu_service_ptr_);
}

// Tests to verify, that if a GpuHost is deleted before GpuClient receives a
// callback, that GpuClient is torn down and does not attempt to use GpuInfo
// after deletion. This should not crash on asan-builds.
TEST_F(GpuHostTest, GpuClientDestructionOrder) {
  base::WeakPtr<GpuClient> client_ref = AddGpuClient();
  EXPECT_NE(nullptr, client_ref);
  DestroyHost();
  EXPECT_EQ(nullptr, client_ref);
}

}  // namespace test
}  // namespace ws
}  // namespace ui
