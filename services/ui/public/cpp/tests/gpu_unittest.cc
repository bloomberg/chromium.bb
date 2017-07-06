// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/public/cpp/gpu/gpu.h"

#include "base/callback_helpers.h"
#include "base/test/scoped_task_environment.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

class TestGpuImpl : public mojom::Gpu {
 public:
  TestGpuImpl() {}
  ~TestGpuImpl() override {}

  mojom::GpuPtr GetPtr() {
    mojom::GpuPtr ptr;
    bindings_.AddBinding(this, mojo::MakeRequest(&ptr));
    return ptr;
  }

  void WaitUntilChannelRequest() {
    DCHECK(quit_closure_.is_null());
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
    quit_closure_ = base::Closure();
  }

  bool RespondToGpuChannelRequests() {
    if (establish_channel_callback_.is_null())
      return false;
    constexpr int client_id = 1;
    mojo::ScopedMessagePipeHandle handle;
    base::ResetAndReturn(&establish_channel_callback_)
        .Run(client_id, std::move(handle), gpu::GPUInfo());
    return true;
  }

  // ui::mojom::Gpu overrides:
  void EstablishGpuChannel(
      const EstablishGpuChannelCallback& callback) override {
    establish_channel_callback_ = callback;
    if (!quit_closure_.is_null())
      quit_closure_.Run();
  }

  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      const ui::mojom::Gpu::CreateGpuMemoryBufferCallback& callback) override {}

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              const gpu::SyncToken& sync_token) override {}

 private:
  EstablishGpuChannelCallback establish_channel_callback_;

  mojo::BindingSet<mojom::Gpu> bindings_;
  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuImpl);
};

}  // namespace

class GpuTest : public testing::Test {
 public:
  GpuTest() {}
  ~GpuTest() override {}

  Gpu* gpu() { return gpu_.get(); }

  TestGpuImpl* gpu_impl() { return &gpu_impl_; }

  // testing::Test:
  void SetUp() override {
    auto factory =
        base::BindRepeating(&TestGpuImpl::GetPtr, base::Unretained(&gpu_impl_));
    gpu_ = base::WrapUnique(new Gpu(std::move(factory), nullptr));
  }

 private:
  base::test::ScopedTaskEnvironment env_;
  TestGpuImpl gpu_impl_;
  std::unique_ptr<Gpu> gpu_;

  DISALLOW_COPY_AND_ASSIGN(GpuTest);
};

// Tests that multiple calls for establishing a gpu channel are all notified
// correctly when the channel is established (or fails to establish).
TEST_F(GpuTest, EstablishRequestsQueued) {
  int counter = 2;
  base::RunLoop run_loop;
  // A callback that decrements the counter, and runs the callback when the
  // counter reaches 0.
  auto callback = base::Bind(
      [](int* counter, const base::Closure& callback,
         scoped_refptr<gpu::GpuChannelHost> channel) {
        --(*counter);
        if (*counter == 0)
          callback.Run();
      },
      &counter, run_loop.QuitClosure());
  gpu()->EstablishGpuChannel(callback);
  gpu()->EstablishGpuChannel(callback);
  EXPECT_EQ(2, counter);
  gpu_impl()->WaitUntilChannelRequest();

  EXPECT_TRUE(gpu_impl()->RespondToGpuChannelRequests());
  run_loop.Run();
  EXPECT_EQ(0, counter);
}

}  // namespace ui
