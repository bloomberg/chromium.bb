// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/composite_host_config.h"

#include "remoting/host/json_host_config.h"

namespace remoting {

CompositeHostConfig::CompositeHostConfig() {
}

CompositeHostConfig::~CompositeHostConfig() {
}

void CompositeHostConfig::AddConfigPath(const FilePath& path) {
  configs_.push_back(new JsonHostConfig(path));
}

bool CompositeHostConfig::Read() {
  bool result = true;
  for (ScopedVector<JsonHostConfig>::iterator it = configs_.begin();
       it != configs_.end(); ++it) {
    result = result && (*it)->Read();
  }
  return result;
}

bool CompositeHostConfig::GetString(const std::string& path,
                                    std::string* out_value) const {
  for (ScopedVector<JsonHostConfig>::const_iterator it = configs_.begin();
       it != configs_.end(); ++it) {
    if ((*it)->GetString(path, out_value))
      return true;
  }
  return false;
}

bool CompositeHostConfig::GetBoolean(const std::string& path,
                                     bool* out_value) const {
  for (ScopedVector<JsonHostConfig>::const_iterator it = configs_.begin();
       it != configs_.end(); ++it) {
    if ((*it)->GetBoolean(path, out_value))
      return true;
  }
  return false;
}

}  // namespace remoting
