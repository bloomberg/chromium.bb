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
#include "remoting/test/app_remoting_connection_helper.h"
#include "remoting/test/app_remoting_test_driver_environment.h"
#include "remoting/test/remote_application_details.h"
#include "remoting/test/test_chromoting_client.h"

namespace {

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
      timer_(new base::Timer(true, false)) {
}

AppRemotingConnectedClientFixture::~AppRemotingConnectedClientFixture() {
}

void AppRemotingConnectedClientFixture::SetUp() {
  connection_helper_.reset(
      new AppRemotingConnectionHelper(application_details_));
  connection_helper_->Initialize();
  if (!connection_helper_->StartConnection()) {
    LOG(ERROR) << "Remote host connection could not be established.";
    FAIL();
  }
}

void AppRemotingConnectedClientFixture::TearDown() {
  connection_helper_.reset();
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

  HostMessageReceivedCallback host_message_received_callback_ =
      base::Bind(&SimpleHostMessageHandler, message_response_title,
                 message_payload, run_loop_->QuitClosure(), &message_received);
  connection_helper_->SetHostMessageReceivedCallback(
      host_message_received_callback_);

  protocol::ExtensionMessage message;
  message.set_type(message_request_title);
  message.set_data(message_payload);
  connection_helper_->host_stub()->DeliverClientMessage(message);

  DCHECK(!timer_->IsRunning());
  timer_->Start(FROM_HERE, max_wait_time, run_loop_->QuitClosure());

  run_loop_->Run();
  timer_->Stop();

  connection_helper_->ResetHostMessageReceivedCallback();
  return message_received;
}

}  // namespace test
}  // namespace remoting
