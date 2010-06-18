// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/json_host_config.h"

#include "base/message_loop_proxy.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_store.h"

namespace remoting {

JsonHostConfig::JsonHostConfig(
    const FilePath& filename,
    base::MessageLoopProxy* file_message_loop_proxy)
    : pref_store_(new JsonPrefStore(filename, file_message_loop_proxy)),
      message_loop_proxy_(file_message_loop_proxy) {
}

bool JsonHostConfig::Read() {
  return pref_store_->ReadPrefs() == PrefStore::PREF_READ_ERROR_NONE;
}

bool JsonHostConfig::GetString(const std::wstring& path,
                               std::wstring* out_value) {
  AutoLock auto_lock(lock_);
  return pref_store_->prefs()->GetString(path, out_value);
}

bool JsonHostConfig::GetString(const std::wstring& path,
                               std::string* out_value) {
  AutoLock auto_lock(lock_);
  return pref_store_->prefs()->GetString(path, out_value);
}

void JsonHostConfig::Update(Task* task) {
  {
    AutoLock auto_lock(lock_);
    task->Run();
  }
  delete task;
  message_loop_proxy_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JsonHostConfig::DoWrite));
}

void JsonHostConfig::SetString(const std::wstring& path,
                               const std::wstring& in_value) {
  lock_.AssertAcquired();
  pref_store_->prefs()->SetString(path, in_value);
}

void JsonHostConfig::SetString(const std::wstring& path,
                               const std::string& in_value) {
  lock_.AssertAcquired();
  pref_store_->prefs()->SetString(path, in_value);
}

void JsonHostConfig::DoWrite() {
  AutoLock auto_lock(lock_);
  pref_store_->WritePrefs();
}

}  // namespace remoting
