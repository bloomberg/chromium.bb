// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/in_memory_host_config.h"

#include "base/logging.h"
#include "base/values.h"

namespace remoting {

InMemoryHostConfig::InMemoryHostConfig()
    : values_(new DictionaryValue()) {
}

InMemoryHostConfig::~InMemoryHostConfig() {}

bool InMemoryHostConfig::GetString(const std::string& path,
                                   std::string* out_value) const {
  DCHECK(CalledOnValidThread());
  return values_->GetString(path, out_value);
}

bool InMemoryHostConfig::GetBoolean(const std::string& path,
                                    bool* out_value) const {
  DCHECK(CalledOnValidThread());
  return values_->GetBoolean(path, out_value);
}

bool InMemoryHostConfig::Save() {
  // Saving in-memory host config is not supported.
  NOTREACHED();
  return false;
}

void InMemoryHostConfig::SetString(const std::string& path,
                                   const std::string& in_value) {
  DCHECK(CalledOnValidThread());
  values_->SetString(path, in_value);
}

void InMemoryHostConfig::SetBoolean(const std::string& path, bool in_value) {
  DCHECK(CalledOnValidThread());
  values_->SetBoolean(path, in_value);
}

bool InMemoryHostConfig::CopyFrom(const base::DictionaryValue* dictionary) {
  bool result = true;

  for (DictionaryValue::key_iterator key(dictionary->begin_keys());
       key != dictionary->end_keys(); ++key) {
    std::string str_value;
    bool bool_value;
    if (dictionary->GetString(*key, &str_value)) {
      SetString(*key, str_value);
    } else if (dictionary->GetBoolean(*key, &bool_value)) {
      SetBoolean(*key, bool_value);
    } else {
      result = false;
    }
  }

  return result;
}

}  // namespace remoting
