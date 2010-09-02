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

bool InMemoryHostConfig::GetString(const std::string& path,
                                   std::string* out_value) {
  AutoLock auto_lock(lock_);
  return values_->GetString(path, out_value);
}

void InMemoryHostConfig::Update(Task* task) {
  {
    AutoLock auto_lock(lock_);
    task->Run();
  }
  delete task;
}

void InMemoryHostConfig::SetString(const std::string& path,
                                   const std::string& in_value) {
  lock_.AssertAcquired();
  values_->SetString(path, in_value);
}

}  // namespace remoting
