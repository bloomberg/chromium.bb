// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/gl_test_setup_helper.h"

#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/tests/gl_test_utils.h"
#include "ui/gl/init/gl_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace gpu {

GLTestSetupHelper::GLTestSetupHelper() {
  viz::TestGpuServiceHolder::DoNotResetOnTestExit();
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  listeners.Append(this);
}

GLTestSetupHelper::~GLTestSetupHelper() {
  testing::TestEventListeners& listeners =
      testing::UnitTest::GetInstance()->listeners();
  CHECK_EQ(this, listeners.Release(this));
}

void GLTestSetupHelper::OnTestStart(const testing::TestInfo& test_info) {
  task_environment_ = std::make_unique<base::test::TaskEnvironment>(
      base::test::TaskEnvironment::MainThreadType::UI);

#if defined(USE_OZONE)
  // Make Ozone run in single-process mode.
  ui::OzonePlatform::InitParams params;
  params.single_process = true;

  // This initialization must be done after TaskEnvironment has
  // initialized the UI thread.
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);
#endif  // defined(USE_OZONE)

  gpu::GLTestHelper::InitializeGLDefault();
  ::gles2::Initialize();
}

void GLTestSetupHelper::OnTestEnd(const testing::TestInfo& test_info) {
  // Explicitly tear down the gpu-service (if active) before shutting down GL.
  // Otherwise the gpu-service tries to access GL during tear-down and causes
  // crashes.
  viz::TestGpuServiceHolder::ResetInstance();
  gl::init::ShutdownGL(/*due_to_fallback=*/false);
  task_environment_ = nullptr;
}

}  // namespace gpu
