// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/chromoting_event.h"

#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/sys_info.h"
#include "remoting/base/name_value_map.h"

namespace remoting {

namespace {

const NameMapElement<ChromotingEvent::Os> kOsNames[] = {
    {ChromotingEvent::Os::CHROMOTING_LINUX, "linux"},
    {ChromotingEvent::Os::CHROMOTING_CHROMEOS, "chromeos"},
    {ChromotingEvent::Os::CHROMOTING_MAC, "mac"},
    {ChromotingEvent::Os::CHROMOTING_WINDOWS, "windows"},
    {ChromotingEvent::Os::CHROMOTING_ANDROID, "android"},
    {ChromotingEvent::Os::CHROMOTING_IOS, "ios"},
};

}  // namespace

const char ChromotingEvent::kCaptureLatencyKey[] = "capture_latency";
const char ChromotingEvent::kConnectionErrorKey[] = "connection_error";
const char ChromotingEvent::kCpuKey[] = "cpu";
const char ChromotingEvent::kDecodeLatencyKey[] = "decode_latency";
const char ChromotingEvent::kEncodeLatencyKey[] = "encode_latency";
const char ChromotingEvent::kHostOsKey[] = "host_os";
const char ChromotingEvent::kHostOsVersionKey[] = "host_os_version";
const char ChromotingEvent::kHostVersionKey[] = "host_version";
const char ChromotingEvent::kMaxCaptureLatencyKey[] = "max_capture_latency";
const char ChromotingEvent::kMaxDecodeLatencyKey[] = "max_decode_latency";
const char ChromotingEvent::kMaxEncodeLatencyKey[] = "max_encode_latency";
const char ChromotingEvent::kMaxRenderLatencyKey[] = "max_render_latency";
const char ChromotingEvent::kMaxRoundtripLatencyKey[] = "max_roundtrip_latency";
const char ChromotingEvent::kModeKey[] = "mode";
const char ChromotingEvent::kOsKey[] = "os";
const char ChromotingEvent::kOsVersionKey[] = "os_version";
const char ChromotingEvent::kRenderLatencyKey[] = "render_latency";
const char ChromotingEvent::kRoleKey[] = "role";
const char ChromotingEvent::kRoundtripLatencyKey[] = "roundtrip_latency";
const char ChromotingEvent::kSessionDurationKey[] = "session_duration";
const char ChromotingEvent::kSessionIdKey[] = "session_id";
const char ChromotingEvent::kSessionStateKey[] = "session_state";
const char ChromotingEvent::kTypeKey[] = "type";
const char ChromotingEvent::kVideoBandwidthKey[] = "video_bandwidth";
const char ChromotingEvent::kWebAppVersionKey[] = "webapp_version";

ChromotingEvent::ChromotingEvent() : values_map_(new base::DictionaryValue()) {}

ChromotingEvent::ChromotingEvent(Type type) : ChromotingEvent() {
  SetEnum(kTypeKey, type);
}

ChromotingEvent::ChromotingEvent(const ChromotingEvent& other) {
  send_attempts_ = other.send_attempts_;
  values_map_ = other.values_map_->CreateDeepCopy();
}

ChromotingEvent::ChromotingEvent(ChromotingEvent&& other) {
  send_attempts_ = other.send_attempts_;
  values_map_ = std::move(other.values_map_);
}

ChromotingEvent::~ChromotingEvent() = default;

ChromotingEvent& ChromotingEvent::operator=(const ChromotingEvent& other) {
  if (this != &other) {
    send_attempts_ = other.send_attempts_;
    values_map_ = other.values_map_->CreateDeepCopy();
  }
  return *this;
}

ChromotingEvent& ChromotingEvent::operator=(ChromotingEvent&& other) {
  send_attempts_ = other.send_attempts_;
  values_map_ = std::move(other.values_map_);
  return *this;
}

void ChromotingEvent::SetString(const std::string& key,
                                const std::string& value) {
  values_map_->SetString(key, value);
}

void ChromotingEvent::SetInteger(const std::string& key, int value) {
  values_map_->SetInteger(key, value);
}

void ChromotingEvent::SetBoolean(const std::string& key, bool value) {
  values_map_->SetBoolean(key, value);
}

void ChromotingEvent::SetDouble(const std::string& key, double value) {
  values_map_->SetDouble(key, value);
}

void ChromotingEvent::AddSystemInfo() {
  SetString(kCpuKey, base::SysInfo::OperatingSystemArchitecture());
  SetString(kOsVersionKey, base::SysInfo::OperatingSystemVersion());
  SetString(kWebAppVersionKey, STRINGIZE(VERSION));
#if defined(OS_LINUX)
  Os os = Os::CHROMOTING_LINUX;
#elif defined(OS_CHROMEOS)
  Os os = Os::CHROMOTING_CHROMEOS;
#elif defined(OS_IOS)
  // This needs to precede the OS_MACOSX check since iOS will also define the
  // OS_MACOSX macro.
  Os os = Os::CHROMOTING_IOS;
#elif defined(OS_MACOSX)
  Os os = Os::CHROMOTING_MAC;
#elif defined(OS_WIN)
  Os os = Os::CHROMOTING_WINDOWS;
#elif defined(OS_ANDROID)
  Os os = Os::CHROMOTING_ANDROID;
#else
  Os os = Os::OTHER;
#endif
  SetEnum(kOsKey, os);
}

void ChromotingEvent::IncrementSendAttempts() {
  send_attempts_++;
}

std::unique_ptr<base::DictionaryValue> ChromotingEvent::CopyDictionaryValue()
    const {
  return values_map_->CreateDeepCopy();
}

// static
bool ChromotingEvent::IsEndOfSession(SessionState state) {
  return state == SessionState::CLOSED ||
         state == SessionState::CONNECTION_DROPPED ||
         state == SessionState::CONNECTION_FAILED ||
         state == SessionState::CONNECTION_CANCELED;
}

// static
ChromotingEvent::Os ChromotingEvent::ParseOsFromString(const std::string& os) {
  ChromotingEvent::Os result;
  if (!NameToValue(kOsNames, base::ToLowerASCII(os), &result)) {
    return Os::OTHER;
  }

  return result;
}

}  // namespace remoting
