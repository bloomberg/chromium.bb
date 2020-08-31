// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONFIG_WATCHER_H_
#define REMOTING_HOST_CONFIG_WATCHER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace remoting {

class ConfigWatcher {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called once after starting watching the configuration file and every time
    // the file changes.
    virtual void OnConfigUpdated(const std::string& serialized_config) = 0;

    // Called when the configuration file watcher encountered an error.
    virtual void OnConfigWatcherError() = 0;
  };

  virtual void Watch(Delegate* delegate) = 0;

  ConfigWatcher() {}

  virtual ~ConfigWatcher() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigWatcher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CONFIG_WATCHER_H_
