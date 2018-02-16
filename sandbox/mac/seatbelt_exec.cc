// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/mac/seatbelt_exec.h"

#include <fcntl.h>
#include <inttypes.h>
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

  int pipe_flags = fcntl(pipe_[1], F_GETFL);
  if (pipe_flags == -1)
    logging::PFatal("SeatbeltExecClient: fctnl(F_GETFL) failed");

  if (fcntl(pipe_[1], F_SETFL, pipe_flags | O_NONBLOCK) == -1)
    logging::PFatal("SeatbeltExecClient: fcntl(F_SETFL) failed");
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
  if (!policy_.SerializeToString(&serialized_protobuf)) {
    logging::Error("SeatbeltExecClient: Serializing the profile failed.");
    return -1;
  }

  if (!WriteString(serialized_protobuf)) {
    logging::Error(
        "SeatbeltExecClient: Writing the serialized profile failed.");
    return -1;
  }

  IGNORE_EINTR(close(pipe_[1]));
  pipe_[1] = -1;

  if (pipe_[0] < 0)
    logging::Error("SeatbeltExecClient: The pipe returned an invalid fd.");

  return pipe_[0];
}

bool SeatbeltExecClient::WriteString(const std::string& str) {
  uint64_t str_len = static_cast<uint64_t>(str.size());

  if (HANDLE_EINTR(write(pipe_[1], &str_len, sizeof(str_len))) !=
      sizeof(str_len)) {
    logging::PError("SeatbeltExecClient: write size of buffer failed");
    return false;
  }

  uint64_t bytes_written = 0;

  while (bytes_written < str_len) {
    ssize_t wrote_this_pass = HANDLE_EINTR(
        write(pipe_[1], &str[bytes_written], str_len - bytes_written));
    if (wrote_this_pass < 0) {
      logging::PError("SeatbeltExecClient: write failed");
      return false;
    }
    bytes_written += wrote_this_pass;
  }

  return true;
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
  uint64_t buf_len = 0;
  if (HANDLE_EINTR(read(fd_, &buf_len, sizeof(buf_len))) != sizeof(buf_len)) {
    logging::PError("SeatbeltExecServer: read buffer length failed");
    return false;
  }

  str->clear();
  str->resize(buf_len);

  uint64_t bytes_read = 0;

  while (bytes_read < buf_len) {
    ssize_t read_this_pass =
        HANDLE_EINTR(read(fd_, &(*str)[bytes_read], buf_len - bytes_read));
    if (read_this_pass < 0) {
      logging::PError("SeatbeltExecServer: read failed");
      return false;
    }
    bytes_read += read_this_pass;
  }

  return true;
}

bool SeatbeltExecServer::SetParameter(const std::string& key,
                                      const std::string& value) {
  return extra_params_.insert(std::make_pair(key, value)).second;
}

}  // namespace sandbox
