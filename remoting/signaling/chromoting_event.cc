// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/chromoting_event.h"

#include "base/sys_info.h"

namespace remoting {

namespace {

const char kKeyType[] = "type";
const char kKeyCpu[] = "cpu";
const char kKeyOs[] = "os";
const char kKeyOsVersion[] = "os_version";

}  // namespace

ChromotingEvent::ChromotingEvent() : values_map_(new base::DictionaryValue()) {}

ChromotingEvent::ChromotingEvent(Type type) : ChromotingEvent() {
  SetEnum(kKeyType, type);
}

ChromotingEvent::ChromotingEvent(const ChromotingEvent& other) {
  try_count_ = other.try_count_;
  values_map_ = other.values_map_->CreateDeepCopy();
}

ChromotingEvent::ChromotingEvent(ChromotingEvent&& other) {
  try_count_ = other.try_count_;
  values_map_ = std::move(other.values_map_);
}

ChromotingEvent::~ChromotingEvent() {}

ChromotingEvent& ChromotingEvent::operator=(const ChromotingEvent& other) {
  if (this != &other) {
    try_count_ = other.try_count_;
    values_map_ = other.values_map_->CreateDeepCopy();
  }
  return *this;
}

ChromotingEvent& ChromotingEvent::operator=(ChromotingEvent&& other) {
  try_count_ = other.try_count_;
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
  SetString(kKeyCpu, base::SysInfo::OperatingSystemArchitecture());
  SetString(kKeyOsVersion, base::SysInfo::OperatingSystemVersion());
  std::string osName = base::SysInfo::OperatingSystemName();
#if defined(OS_LINUX)
  Os os = Os::CHROMOTING_LINUX;
#elif defined(OS_CHROMEOS)
  Os os = Os::CHROMOTING_CHROMEOS;
#elif defined(OS_MACOSX)
  Os os = Os::CHROMOTING_MAC;
#elif defined(OS_WIN)
  Os os = Os::CHROMOTING_WINDOWS;
#elif defined(OS_ANDROID)
  Os os = Os::CHROMOTING_ANDROID;
#elif defined(OS_IOS)
  Os os = Os::CHROMOTING_IOS;
#else
  Os os = Os::OTHER;
#endif
  SetEnum(kKeyOs, os);
}

void ChromotingEvent::IncrementTryCount() {
  try_count_++;
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

}  // namespace remoting
