// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/in_memory_host_config.h"

#include "base/task.h"
#include "base/values.h"

namespace remoting {

InMemoryHostConfig::InMemoryHostConfig()
    : values_(new DictionaryValue()) {
}

InMemoryHostConfig::~InMemoryHostConfig() {}

bool InMemoryHostConfig::GetString(const std::string& path,
                                   std::string* out_value) {
  base::AutoLock auto_lock(lock_);
  return values_->GetString(path, out_value);
}

bool InMemoryHostConfig::GetBoolean(const std::string& path, bool* out_value) {
  base::AutoLock auto_lock(lock_);
  return values_->GetBoolean(path, out_value);
}

void InMemoryHostConfig::Save() {
  // Save is NOP for in-memory host config.
}

void InMemoryHostConfig::SetString(const std::string& path,
                                   const std::string& in_value) {
  base::AutoLock auto_lock(lock_);
  values_->SetString(path, in_value);
}

void InMemoryHostConfig::SetBoolean(const std::string& path, bool in_value) {
  base::AutoLock auto_lock(lock_);
  values_->SetBoolean(path, in_value);
}

}  // namespace remoting
