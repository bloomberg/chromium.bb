// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/json_host_config.h"

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/values.h"

namespace remoting {

JsonHostConfig::JsonHostConfig(
    const FilePath& filename,
    base::MessageLoopProxy* file_message_loop_proxy)
    : filename_(filename),
      message_loop_proxy_(file_message_loop_proxy) {
}

JsonHostConfig::~JsonHostConfig() {}

bool JsonHostConfig::Read() {
  // TODO(sergeyu): Implement better error handling here.
  std::string file_content;
  if (!file_util::ReadFileToString(filename_, &file_content)) {
    return false;
  }

  scoped_ptr<Value> value(base::JSONReader::Read(file_content, true));
  if (value.get() == NULL || !value->IsType(Value::TYPE_DICTIONARY)) {
    return false;
  }

  DictionaryValue* dictionary = static_cast<DictionaryValue*>(value.release());
  base::AutoLock auto_lock(lock_);
  values_.reset(dictionary);
  return true;
}

void JsonHostConfig::Save() {
  message_loop_proxy_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JsonHostConfig::DoWrite));
}

void JsonHostConfig::DoWrite() {
  std::string file_content;
  base::AutoLock auto_lock(lock_);
  base::JSONWriter::Write(values_.get(), true, &file_content);
  // TODO(sergeyu): Move ImportantFileWriter to base and use it here.
  file_util::WriteFile(filename_, file_content.c_str(), file_content.size());
}

}  // namespace remoting
