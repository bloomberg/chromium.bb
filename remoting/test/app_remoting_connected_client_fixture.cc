// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/app_remoting_connected_client_fixture.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/test/app_remoting_test_driver_environment.h"
#include "remoting/test/remote_application_details.h"
#include "remoting/test/test_chromoting_client.h"

namespace {
const int kDefaultDPI = 96;
const int kDefaultWidth = 1024;
const int kDefaultHeight = 768;

const char kHostProcessWindowTitle[] = "Host Process";

void SimpleHostMessageHandler(
    const std::string& target_message_type,
    const std::string& target_message_data,
    const base::Closure& done_closure,
    bool* message_received,
    const remoting::protocol::ExtensionMessage& message) {
  if (message.type() == target_message_type &&
      message.data() == target_message_data) {
    *message_received = true;
    done_closure.Run();
  }
}
}  // namespace

namespace remoting {
namespace test {

AppRemotingConnectedClientFixture::AppRemotingConnectedClientFixture()
    : application_details_(
          AppRemotingSharedData->GetDetailsFromAppName(GetParam())),
      connection_is_ready_for_tests_(false),
      timer_(new base::Timer(true, false)) {
}

AppRemotingConnectedClientFixture::~AppRemotingConnectedClientFixture() {
}

void AppRemotingConnectedClientFixture::SetUp() {
  client_.reset(new TestChromotingClient());
  client_->AddRemoteConnectionObserver(this);

  StartConnection();

  if (!connection_is_ready_for_tests_) {
    LOG(ERROR) << "Remote host connection could not be established.";
    client_->EndConnection();
    FAIL();
  }
}

void AppRemotingConnectedClientFixture::TearDown() {
  // |client_| destroys some of its members via DeleteSoon on the message loop's
  // TaskRunner so we need to run the loop until it has no more work to do.
  client_->RemoveRemoteConnectionObserver(this);
  client_.reset();

  base::RunLoop().RunUntilIdle();
}

bool AppRemotingConnectedClientFixture::VerifyResponseForSimpleHostMessage(
    const std::string& message_request_title,
    const std::string& message_response_title,
    const std::string& message_payload,
    const base::TimeDelta& max_wait_time) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool message_received = false;

  DCHECK(!run_loop_ || !run_loop_->running());
  run_loop_.reset(new base::RunLoop());

  host_message_received_callback_ =
      base::Bind(&SimpleHostMessageHandler, message_response_title,
                 message_payload, run_loop_->QuitClosure(), &message_received);

  protocol::ExtensionMessage message;
  message.set_type(message_request_title);
  message.set_data(message_payload);
  client_->host_stub()->DeliverClientMessage(message);

  DCHECK(!timer_->IsRunning());
  timer_->Start(FROM_HERE, max_wait_time, run_loop_->QuitClosure());

  run_loop_->Run();
  timer_->Stop();

  host_message_received_callback_.Reset();

  return message_received;
}

void AppRemotingConnectedClientFixture::StartConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());

  RemoteHostInfo remote_host_info;
  remoting::test::AppRemotingSharedData->GetRemoteHostInfoForApplicationId(
      application_details_.application_id, &remote_host_info);

  if (!remote_host_info.IsReadyForConnection()) {
    LOG(ERROR) << "Remote Host is unavailable for connections.";
    return;
  }

  DCHECK(!run_loop_ || !run_loop_->running());
  run_loop_.reset(new base::RunLoop());

  // We will wait up to 30 seconds to complete the remote connection and for the
  // main application window to become visible.
  DCHECK(!timer_->IsRunning());
  timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(30),
                run_loop_->QuitClosure());

  client_->StartConnection(AppRemotingSharedData->user_name(),
                           AppRemotingSharedData->access_token(),
                           remote_host_info);

  run_loop_->Run();
  timer_->Stop();
}

void AppRemotingConnectedClientFixture::ConnectionStateChanged(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error_code) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If the connection is closed or failed then mark the connection as closed
  // and quit the current RunLoop if it exists.
  if (state == protocol::ConnectionToHost::CLOSED ||
      state == protocol::ConnectionToHost::FAILED ||
      error_code != protocol::OK) {
    connection_is_ready_for_tests_ = false;

    if (run_loop_) {
      run_loop_->Quit();
    }
  }
}

void AppRemotingConnectedClientFixture::ConnectionReady(bool ready) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (ready) {
    SendClientConnectionDetailsToHost();
  } else {
    // We will only get called here with a false value for |ready| if the video
    // renderer encounters an error.
    connection_is_ready_for_tests_ = false;

    if (run_loop_) {
      run_loop_->Quit();
    }
  }
}

void AppRemotingConnectedClientFixture::HostMessageReceived(
    const protocol::ExtensionMessage& message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If a callback is not registered, then the message is passed to a default
  // handler for the class based on the message type.
  if (!host_message_received_callback_.is_null()) {
    host_message_received_callback_.Run(message);
  } else if (message.type() == "onWindowAdded") {
    HandleOnWindowAddedMessage(message);
  } else {
    DVLOG(2) << "HostMessage not handled by HostMessageReceived().";
    DVLOG(2) << "type: " << message.type();
    DVLOG(2) << "data: " << message.data();
  }
}

void AppRemotingConnectedClientFixture::SendClientConnectionDetailsToHost() {
  // First send an access token which will be used for Google Drive access.
  protocol::ExtensionMessage message;
  message.set_type("accessToken");
  message.set_data(AppRemotingSharedData->access_token());

  DVLOG(1) << "Sending access token to host";
  client_->host_stub()->DeliverClientMessage(message);

  // Next send the host a description of the client screen size.
  protocol::ClientResolution client_resolution;
  client_resolution.set_width(kDefaultWidth);
  client_resolution.set_height(kDefaultHeight);
  client_resolution.set_x_dpi(kDefaultDPI);
  client_resolution.set_y_dpi(kDefaultDPI);
  client_resolution.set_dips_width(kDefaultWidth);
  client_resolution.set_dips_height(kDefaultHeight);

  DVLOG(1) << "Sending ClientResolution details to host";
  client_->host_stub()->NotifyClientResolution(client_resolution);

  // Finally send a message to start sending us video packets.
  protocol::VideoControl video_control;
  video_control.set_enable(true);

  DVLOG(1) << "Sending enable VideoControl message to host";
  client_->host_stub()->ControlVideo(video_control);
}

void AppRemotingConnectedClientFixture::HandleOnWindowAddedMessage(
    const remoting::protocol::ExtensionMessage& message) {
  DCHECK_EQ(message.type(), "onWindowAdded");

  const base::DictionaryValue* message_data = nullptr;
  scoped_ptr<base::Value> host_message = base::JSONReader::Read(message.data());
  if (!host_message.get() || !host_message->GetAsDictionary(&message_data)) {
    LOG(ERROR) << "onWindowAdded message received was not valid JSON.";
    if (run_loop_) {
      run_loop_->Quit();
    }
    return;
  }

  std::string current_window_title;
  message_data->GetString("title", &current_window_title);
  if (current_window_title == kHostProcessWindowTitle) {
    LOG(ERROR) << "Host Process Window is visible, this likely means that the "
               << "underlying application is in a bad state, YMMV.";
  }

  std::string main_window_title = application_details_.main_window_title;
  if (current_window_title.find_first_of(main_window_title) == 0) {
    connection_is_ready_for_tests_ = true;

    if (timer_->IsRunning()) {
      timer_->Stop();
    }

    DCHECK(run_loop_);
    // Now that the main window is visible, give the app some time to settle
    // before signaling that it is ready to run tests.
    timer_->Start(FROM_HERE, base::TimeDelta::FromSeconds(2),
                  run_loop_->QuitClosure());
  }
}

}  // namespace test
}  // namespace remoting
