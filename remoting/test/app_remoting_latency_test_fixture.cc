// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/app_remoting_latency_test_fixture.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/input_stub.h"
#include "remoting/protocol/usb_key_codes.h"
#include "remoting/test/app_remoting_connection_helper.h"
#include "remoting/test/app_remoting_test_driver_environment.h"
#include "remoting/test/rgb_value.h"
#include "remoting/test/test_chromoting_client.h"
#include "remoting/test/test_video_renderer.h"

namespace remoting {
namespace test {

AppRemotingLatencyTestFixture::AppRemotingLatencyTestFixture()
    : timer_(new base::Timer(true, false)) {
  // NOTE: Derived fixture must initialize application details in constructor.
}

AppRemotingLatencyTestFixture::~AppRemotingLatencyTestFixture() {
}

void AppRemotingLatencyTestFixture::SetUp() {
  scoped_ptr<TestVideoRenderer> test_video_renderer(new TestVideoRenderer());
  test_video_renderer_ = test_video_renderer->GetWeakPtr();

  scoped_ptr<TestChromotingClient> test_chromoting_client(
      new TestChromotingClient(test_video_renderer.Pass()));

  test_chromoting_client->AddRemoteConnectionObserver(this);

  connection_helper_.reset(
      new AppRemotingConnectionHelper(GetApplicationDetails()));
  connection_helper_->Initialize(test_chromoting_client.Pass());

  if (!connection_helper_->StartConnection()) {
    LOG(ERROR) << "Remote host connection could not be established.";
    FAIL();
  }

  if (!PrepareApplicationForTesting()) {
    LOG(ERROR) << "Unable to prepare application for testing.";
    FAIL();
  }
}

void AppRemotingLatencyTestFixture::TearDown() {
  // Only reset application state when remote host connection is established.
  if (connection_helper_->ConnectionIsReadyForTest()) {
    ResetApplicationState();
  }

  connection_helper_->test_chromoting_client()->RemoveRemoteConnectionObserver(
      this);
  connection_helper_.reset();
}

WaitForImagePatternMatchCallback
AppRemotingLatencyTestFixture::SetExpectedImagePattern(
    const webrtc::DesktopRect& expected_rect,
    const RGBValue& expected_color) {
  scoped_ptr<base::RunLoop> run_loop(new base::RunLoop());

  test_video_renderer_->ExpectAverageColorInRect(expected_rect, expected_color,
                                                 run_loop->QuitClosure());

  return base::Bind(&AppRemotingLatencyTestFixture::WaitForImagePatternMatch,
                    base::Unretained(this), base::Passed(&run_loop));
}

void AppRemotingLatencyTestFixture::SaveFrameDataToDisk(
    bool save_frame_data_to_disk) {
  test_video_renderer_->SaveFrameDataToDisk(save_frame_data_to_disk);
}

bool AppRemotingLatencyTestFixture::WaitForImagePatternMatch(
    scoped_ptr<base::RunLoop> run_loop,
    const base::TimeDelta& max_wait_time) {
  DCHECK(run_loop);
  DCHECK(!timer_->IsRunning());

  timer_->Start(FROM_HERE, max_wait_time, run_loop->QuitClosure());

  run_loop->Run();

  // Image pattern is matched if we stopped because of the reply not the timer.
  bool image_pattern_is_matched = (timer_->IsRunning());
  timer_->Stop();
  run_loop.reset();

  return image_pattern_is_matched;
}

void AppRemotingLatencyTestFixture::HostMessageReceived(
    const protocol::ExtensionMessage& message) {
  if (!host_message_received_callback_.is_null()) {
    host_message_received_callback_.Run(message);
  }
}

void AppRemotingLatencyTestFixture::PressKey(uint32_t usb_keycode,
                                             bool pressed) {
  remoting::protocol::KeyEvent event;
  event.set_usb_keycode(usb_keycode);
  event.set_pressed(pressed);
  connection_helper_->input_stub()->InjectKeyEvent(event);
}

void AppRemotingLatencyTestFixture::PressAndReleaseKey(uint32_t usb_keycode) {
  PressKey(usb_keycode, true);
  PressKey(usb_keycode, false);
}

void AppRemotingLatencyTestFixture::PressAndReleaseKeyCombination(
    const std::vector<uint32_t>& usb_keycodes) {
  for (std::vector<uint32_t>::const_iterator iter = usb_keycodes.begin();
       iter != usb_keycodes.end(); ++iter) {
    PressKey(*iter, true);
  }
  for (std::vector<uint32_t>::const_reverse_iterator iter =
           usb_keycodes.rbegin();
       iter != usb_keycodes.rend(); ++iter) {
    PressKey(*iter, false);
  }
}

void AppRemotingLatencyTestFixture::SetHostMessageReceivedCallback(
    const HostMessageReceivedCallback& host_message_received_callback) {
  host_message_received_callback_ = host_message_received_callback;
}

void AppRemotingLatencyTestFixture::ResetHostMessageReceivedCallback() {
  host_message_received_callback_.Reset();
}

void AppRemotingLatencyTestFixture::ResetApplicationState() {
  DCHECK(!timer_->IsRunning());
  DCHECK(!run_loop_ || !run_loop_->running());

  // Give the app some time to settle before reseting to initial state.
  run_loop_.reset(new base::RunLoop());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop_->QuitClosure(), base::TimeDelta::FromSeconds(1));
  run_loop_->Run();

  // Press Alt + F4 and wait for amount of time for the input to be delivered
  // and processed.
  std::vector<uint32_t> usb_keycodes;
  usb_keycodes.push_back(kUsbLeftAlt);
  usb_keycodes.push_back(kUsbF4);
  PressAndReleaseKeyCombination(usb_keycodes);

  run_loop_.reset(new base::RunLoop());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop_->QuitClosure(), base::TimeDelta::FromSeconds(2));
  run_loop_->Run();

  // Press 'N' to choose not save and wait for 1 second for the input to be
  // delivered and processed.
  PressAndReleaseKey(kUsbN);

  run_loop_.reset(new base::RunLoop());
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop_->QuitClosure(), base::TimeDelta::FromSeconds(2));
  run_loop_->Run();
  run_loop_.reset();
}

}  // namespace test
}  // namespace remoting
