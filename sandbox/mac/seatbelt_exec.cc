// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt_exec.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <vector>

#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"
#include "sandbox/mac/sandbox_logging.h"
#include "sandbox/mac/seatbelt.h"

namespace sandbox {

SeatbeltExecClient::SeatbeltExecClient() {
  if (pipe(pipe_) != 0)
    logging::PFatal("SeatbeltExecClient: pipe failed");
}

SeatbeltExecClient::~SeatbeltExecClient() {
  if (pipe_[1] != -1)
    IGNORE_EINTR(close(pipe_[1]));
  // If pipe() fails, PCHECK() will be hit in the constructor, so this file
  // descriptor should always be closed if the proess is alive at this point.
  IGNORE_EINTR(close(pipe_[0]));
}

bool SeatbeltExecClient::SetBooleanParameter(const std::string& key,
                                             bool value) {
  google::protobuf::MapPair<std::string, std::string> pair(
      key, value ? "TRUE" : "FALSE");
  return policy_.mutable_params()->insert(pair).second;
}

bool SeatbeltExecClient::SetParameter(const std::string& key,
                                      const std::string& value) {
  google::protobuf::MapPair<std::string, std::string> pair(key, value);
  return policy_.mutable_params()->insert(pair).second;
}

void SeatbeltExecClient::SetProfile(const std::string& policy) {
  policy_.set_profile(policy);
}

int SeatbeltExecClient::SendProfileAndGetFD() {
  std::string serialized_protobuf;
  if (!policy_.SerializeToString(&serialized_protobuf))
    return -1;

  if (!WriteString(&serialized_protobuf))
    return -1;

  IGNORE_EINTR(close(pipe_[1]));
  pipe_[1] = -1;

  return pipe_[0];
}

bool SeatbeltExecClient::WriteString(std::string* str) {
  struct iovec iov[1];
  iov[0].iov_base = &(*str)[0];
  iov[0].iov_len = str->size();

  ssize_t written = HANDLE_EINTR(writev(pipe_[1], iov, arraysize(iov)));
  if (written < 0) {
    logging::PError("SeatbeltExecClient: writev failed");
    return false;
  }
  return static_cast<uint64_t>(written) == str->size();
}

SeatbeltExecServer::SeatbeltExecServer(int fd) : fd_(fd), extra_params_() {}

SeatbeltExecServer::~SeatbeltExecServer() {
  close(fd_);
}

bool SeatbeltExecServer::InitializeSandbox() {
  std::string policy_string;
  if (!ReadString(&policy_string))
    return false;

  mac::SandboxPolicy policy;
  if (!policy.ParseFromString(policy_string)) {
    logging::Error("SeatbeltExecServer: ParseFromString failed");
    return false;
  }

  return ApplySandboxProfile(policy);
}

bool SeatbeltExecServer::ApplySandboxProfile(const mac::SandboxPolicy& policy) {
  std::vector<const char*> weak_params;
  for (const auto& pair : policy.params()) {
    weak_params.push_back(pair.first.c_str());
    weak_params.push_back(pair.second.c_str());
  }
  for (const auto& pair : extra_params_) {
    weak_params.push_back(pair.first.c_str());
    weak_params.push_back(pair.second.c_str());
  }
  weak_params.push_back(nullptr);

  char* error = nullptr;
  int rv = Seatbelt::InitWithParams(policy.profile().c_str(), 0,
                                    weak_params.data(), &error);
  if (error) {
    logging::Error("SeatbeltExecServer: Failed to initialize sandbox: %d %s",
                   rv, error);
    Seatbelt::FreeError(error);
    return false;
  }

  return rv == 0;
}

bool SeatbeltExecServer::ReadString(std::string* str) {
  // 4 pages of memory is enough to hold the sandbox profiles.
  std::vector<char> buffer(4096 * 4, '\0');

  struct iovec iov[1];
  iov[0].iov_base = buffer.data();
  iov[0].iov_len = buffer.size();

  ssize_t read_length = HANDLE_EINTR(readv(fd_, iov, arraysize(iov)));
  if (read_length < 0) {
    logging::PError("SeatbeltExecServer: readv failed");
    return false;
  }
  str->assign(buffer.data());
  return true;
}

bool SeatbeltExecServer::SetParameter(const std::string& key,
                                      const std::string& value) {
  return extra_params_.insert(std::make_pair(key, value)).second;
}

}  // namespace sandbox
