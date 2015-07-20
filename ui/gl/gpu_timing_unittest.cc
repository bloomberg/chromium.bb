// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub_with_extensions.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gpu_preference.h"
#include "ui/gl/gpu_timing.h"
#include "ui/gl/gpu_timing_fake.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gfx {

class GPUTimingTest : public testing::Test {
 public:
  void SetUp() override {
    setup_ = false;
    fake_cpu_time_ = 0;
    cpu_time_bounded_ = false;
  }

  void TearDown() override {
    if (setup_) {
      MockGLInterface::SetGLInterface(NULL);
      gfx::ClearGLBindings();
    }
    setup_ = false;
    cpu_time_bounded_ = false;
    context_ = nullptr;
    gl_.reset();
    gpu_timing_fake_queries_.Reset();
  }

  void SetupGLContext(const char* gl_version, const char* gl_extensions) {
    ASSERT_FALSE(setup_) << "Cannot setup GL context twice.";
    SetGLGetProcAddressProc(MockGLInterface::GetGLProcAddress);
    GLSurfaceTestSupport::InitializeOneOffWithMockBindings();
    gl_.reset(new ::testing::StrictMock<MockGLInterface>());
    MockGLInterface::SetGLInterface(gl_.get());

    context_ = new GLContextStubWithExtensions;
    context_->AddExtensionsString(gl_extensions);
    context_->SetGLVersionString(gl_version);
    gpu_timing_fake_queries_.Reset();
    GLSurfaceTestSupport::InitializeDynamicMockBindings(context_.get());

    setup_ = true;
  }

  scoped_refptr<GPUTimingClient> CreateGPUTimingClient() {
    if (!setup_) {
      SetupGLContext("2.0", "");
    }

    scoped_refptr<GPUTimingClient> client = context_->CreateGPUTimingClient();
    if (!cpu_time_bounded_) {
      client->SetCpuTimeForTesting(base::Bind(&GetFakeCPUTime));
      cpu_time_bounded_ = true;
    }
    return client;
  }

  void SetFakeCPUTime(int64_t fake_cpu_time) {
    fake_cpu_time_ = fake_cpu_time;
  }

 protected:
  static int64_t GetFakeCPUTime() {
    return fake_cpu_time_;
  }

  static int64_t fake_cpu_time_;

  bool setup_ = false;
  bool cpu_time_bounded_ = false;
  scoped_ptr< ::testing::StrictMock<MockGLInterface> > gl_;
  scoped_refptr<GLContextStubWithExtensions> context_;
  GPUTimingFake gpu_timing_fake_queries_;
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

TEST_F(GPUTimingTest, QueryTimeStampTest) {
  SetupGLContext("3.2", "GL_ARB_timer_query");
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();
  scoped_ptr<GPUTimer> gpu_timer = client->CreateGPUTimer(false);

  SetFakeCPUTime(123);
  gpu_timing_fake_queries_.SetCurrentGLTime(
      10 * base::Time::kNanosecondsPerMicrosecond);
  gpu_timing_fake_queries_.ExpectGPUTimeStampQuery(*gl_, false);

  gpu_timer->QueryTimeStamp();

  SetFakeCPUTime(122);
  gpu_timing_fake_queries_.SetCurrentGLTime(
      9 * base::Time::kNanosecondsPerMicrosecond);
  EXPECT_FALSE(gpu_timer->IsAvailable());

  SetFakeCPUTime(124);
  gpu_timing_fake_queries_.SetCurrentGLTime(
      11 * base::Time::kNanosecondsPerMicrosecond);
  EXPECT_TRUE(gpu_timer->IsAvailable());
  EXPECT_EQ(0, gpu_timer->GetDeltaElapsed());

  int64 start, end;
  gpu_timer->GetStartEndTimestamps(&start, &end);
  EXPECT_EQ(123, start);
  EXPECT_EQ(123, end);
}

TEST_F(GPUTimingTest, QueryTimeStampUsingElapsedTest) {
  // Test timestamp queries using GL_EXT_timer_query which does not support
  // timestamp queries. Internally we fall back to time elapsed queries.
  SetupGLContext("3.2", "GL_EXT_timer_query");
  scoped_refptr<GPUTimingClient> client = CreateGPUTimingClient();
  scoped_ptr<GPUTimer> gpu_timer = client->CreateGPUTimer(false);
  ASSERT_TRUE(client->IsForceTimeElapsedQuery());

  SetFakeCPUTime(123);
  gpu_timing_fake_queries_.SetCurrentGLTime(
      10 * base::Time::kNanosecondsPerMicrosecond);
  gpu_timing_fake_queries_.ExpectGPUTimeStampQuery(*gl_, true);

  gpu_timer->QueryTimeStamp();

  SetFakeCPUTime(122);
  gpu_timing_fake_queries_.SetCurrentGLTime(
      9 * base::Time::kNanosecondsPerMicrosecond);
  EXPECT_FALSE(gpu_timer->IsAvailable());

  SetFakeCPUTime(124);
  gpu_timing_fake_queries_.SetCurrentGLTime(
      11 * base::Time::kNanosecondsPerMicrosecond);
  EXPECT_TRUE(gpu_timer->IsAvailable());
  EXPECT_EQ(0, gpu_timer->GetDeltaElapsed());

  int64 start, end;
  gpu_timer->GetStartEndTimestamps(&start, &end);
  EXPECT_EQ(123, start);
  EXPECT_EQ(123, end);
}

}  // namespace gpu
