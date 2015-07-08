// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/app_remoting_latency_test_fixture.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "remoting/test/app_remoting_connection_helper.h"
#include "remoting/test/app_remoting_test_driver_environment.h"
#include "remoting/test/test_chromoting_client.h"
#include "remoting/test/test_video_renderer.h"

namespace remoting {
namespace test {

AppRemotingLatencyTestFixture::AppRemotingLatencyTestFixture()
    : application_details_(
          AppRemotingSharedData->GetDetailsFromAppName(application_name_)),
      timer_(new base::Timer(true, false)) {
  // NOTE: Derived fixture must initialize application name in constructor.
}

AppRemotingLatencyTestFixture::~AppRemotingLatencyTestFixture() {
}

void AppRemotingLatencyTestFixture::SetUp() {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_ptr<TestVideoRenderer> test_video_renderer(new TestVideoRenderer());
  test_video_renderer_ = test_video_renderer->GetWeakPtr();

  scoped_ptr<TestChromotingClient> test_chromoting_client(
      new TestChromotingClient(test_video_renderer.Pass()));

  connection_helper_.reset(
      new AppRemotingConnectionHelper(application_details_));
  connection_helper_->Initialize(test_chromoting_client.Pass());

  if (!connection_helper_->StartConnection()) {
    LOG(ERROR) << "Remote host connection could not be established.";
    FAIL();
  }
}

void AppRemotingLatencyTestFixture::TearDown() {
  connection_helper_.reset();
}

void AppRemotingLatencyTestFixture::SetExpectedImagePattern(
    const webrtc::DesktopRect& expected_rect,
    const RgbaColor& expected_color) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!run_loop_ || !run_loop_->running());

  run_loop_.reset(new base::RunLoop());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&TestVideoRenderer::SetImagePatternAndMatchedCallback,
                 test_video_renderer_, expected_rect, expected_color,
                 run_loop_->QuitClosure()));
}

bool AppRemotingLatencyTestFixture::WaitForImagePatternMatched(
    const base::TimeDelta& max_wait_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(run_loop_);

  DCHECK(!timer_->IsRunning());
  timer_->Start(FROM_HERE, max_wait_time, run_loop_->QuitClosure());

  run_loop_->Run();

  // Image pattern is matched if we stopped because of the reply not the timer.
  bool image_pattern_is_matched = (timer_->IsRunning());
  timer_->Stop();
  run_loop_.reset();

  return image_pattern_is_matched;
}

}  // namespace test
}  // namespace remoting
