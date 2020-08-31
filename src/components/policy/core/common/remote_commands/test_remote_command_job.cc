// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/remote_commands/test_remote_command_job.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/check_op.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace policy {

const int kCommandExpirationTimeInHours = 3;

namespace em = enterprise_management;

const char TestRemoteCommandJob::kMalformedCommandPayload[] =
    "_MALFORMED_COMMAND_PAYLOAD_";

class TestRemoteCommandJob::EchoPayload
    : public RemoteCommandJob::ResultPayload {
 public:
  explicit EchoPayload(const std::string& payload) : payload_(payload) {}

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  const std::string payload_;

  DISALLOW_COPY_AND_ASSIGN(EchoPayload);
};

std::unique_ptr<std::string> TestRemoteCommandJob::EchoPayload::Serialize() {
  return std::make_unique<std::string>(payload_);
}

TestRemoteCommandJob::TestRemoteCommandJob(bool succeed,
                                           base::TimeDelta execution_duration)
    : succeed_(succeed), execution_duration_(execution_duration) {
  DCHECK_LT(base::TimeDelta::FromSeconds(0), execution_duration_);
}

em::RemoteCommand_Type TestRemoteCommandJob::GetType() const {
  return em::RemoteCommand_Type_COMMAND_ECHO_TEST;
}

bool TestRemoteCommandJob::ParseCommandPayload(
    const std::string& command_payload) {
  if (command_payload == kMalformedCommandPayload)
    return false;
  command_payload_ = command_payload;
  return true;
}

bool TestRemoteCommandJob::IsExpired(base::TimeTicks now) {
  return !issued_time().is_null() &&
         now > issued_time() +
                   base::TimeDelta::FromHours(kCommandExpirationTimeInHours);
}

void TestRemoteCommandJob::RunImpl(CallbackWithResult succeed_callback,
                                   CallbackWithResult failed_callback) {
  std::unique_ptr<ResultPayload> echo_payload(
      new EchoPayload(command_payload_));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          succeed_ ? std::move(succeed_callback) : std::move(failed_callback),
          std::move(echo_payload)),
      execution_duration_);
}

}  // namespace policy
