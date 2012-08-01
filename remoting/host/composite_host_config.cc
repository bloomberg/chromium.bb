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

bool CompositeHostConfig::AddConfigPath(const FilePath& path) {
  scoped_ptr<JsonHostConfig> config(new JsonHostConfig(path));
  if (!config->Read()) {
    return false;
  }
  configs_.push_back(config.release());
  return true;
}

bool CompositeHostConfig::GetString(const std::string& path,
                                    std::string* out_value) const {
  for (std::vector<HostConfig*>::const_iterator it = configs_.begin();
       it != configs_.end(); ++it) {
    if ((*it)->GetString(path, out_value))
      return true;
  }
  return false;
}

bool CompositeHostConfig::GetBoolean(const std::string& path,
                                     bool* out_value) const {
  for (std::vector<HostConfig*>::const_iterator it = configs_.begin();
       it != configs_.end(); ++it) {
    if ((*it)->GetBoolean(path, out_value))
      return true;
  }
  return false;
}

}  // namespace remoting
