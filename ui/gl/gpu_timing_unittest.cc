// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_timing.h"

namespace gfx {

class GPUTimingTest : public testing::Test {
 public:
  void SetUp() override {
    setup_ = false;
    fake_cpu_time_ = 0;

    CreateGPUTimingClient()->SetCpuTimeForTesting(base::Bind(&GetFakeCPUTime));
  }

  void TearDown() override {
    context_ = nullptr;
  }

  void SetupGLContext(const char* gl_version, const char* gl_extensions) {
    ASSERT_FALSE(setup_) << "Cannot setup GL context twice.";
    gfx::SetGLGetProcAddressProc(gfx::MockGLInterface::GetGLProcAddress);
    gfx::GLSurface::InitializeOneOffWithMockBindingsForTests();
    gl_.reset(new ::testing::StrictMock< ::gfx::MockGLInterface>());
    ::gfx::MockGLInterface::SetGLInterface(gl_.get());

    context_ = new gfx::GLContextStubWithExtensions;
    context_->AddExtensionsString(gl_extensions);
    context_->SetGLVersionString(gl_version);

    setup_ = true;
  }

  scoped_refptr<GPUTimingClient> CreateGPUTimingClient() {
    if (!setup_) {
      SetupGLContext("2.0", "");
    }
    return context_->CreateGPUTimingClient();
  }

  void SetFakeCPUTime(int64_t fake_cpu_time) {
    fake_cpu_time_ = fake_cpu_time;
  }

 protected:
  static int64_t GetFakeCPUTime() {
    return fake_cpu_time_;
  }

 private:
  static int64_t fake_cpu_time_;

  bool setup_ = false;
  scoped_ptr< ::testing::StrictMock< ::gfx::MockGLInterface> > gl_;
  scoped_refptr<gfx::GLContextStubWithExtensions> context_;
};

int64_t GPUTimingTest::fake_cpu_time_ = 0;

TEST_F(GPUTimingTest, FakeTimerTest) {
  // Tests that we can properly set fake cpu times.
  SetFakeCPUTime(123);

  scoped_refptr<GPUTimingClient> gpu_timing_client = CreateGPUTimingClient();
  EXPECT_EQ(123, gpu_timing_client->GetCurrentCPUTime());

  base::Callback<int64_t(void)> empty;
  gpu_timing_client->SetCpuTimeForTesting(empty);
  EXPECT_NE(123, gpu_timing_client->GetCurrentCPUTime());
}

TEST_F(GPUTimingTest, ForceTimeElapsedQuery) {
  // Test that forcing time elapsed query affects all clients.
  scoped_refptr<GPUTimingClient> client1 = CreateGPUTimingClient();
  EXPECT_FALSE(client1->IsForceTimeElapsedQuery());

  scoped_refptr<GPUTimingClient> client_force = CreateGPUTimingClient();
  EXPECT_FALSE(client1->IsForceTimeElapsedQuery());
  client_force->ForceTimeElapsedQuery();
  EXPECT_TRUE(client1->IsForceTimeElapsedQuery());

  EXPECT_TRUE(client1->IsForceTimeElapsedQuery());

  scoped_refptr<GPUTimingClient> client2 = CreateGPUTimingClient();
  EXPECT_TRUE(client2->IsForceTimeElapsedQuery());
}

}  // namespace gpu
