// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_CONFIG_WATCHER_H_
#define REMOTING_HOST_DAEMON_CONFIG_WATCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

extern const char kHostConfigSwitchName[];
extern const base::FilePath::CharType kDefaultHostConfigFile[];

class ConfigFileWatcherImpl;

class ConfigFileWatcher {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Called once after starting watching the configuration file and every time
    // the file changes.
    virtual void OnConfigUpdated(const std::string& serialized_config) = 0;

    // Called when the configuration file watcher encountered an error.
    virtual void OnConfigWatcherError() = 0;
  };

  // Creates a configuration file watcher that lives at the |io_task_runner|
  // thread but posts config file updates on on |main_task_runner|.
  ConfigFileWatcher(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      Delegate* delegate);
  virtual ~ConfigFileWatcher();

  // Starts watching |config_path|.
  void Watch(const base::FilePath& config_path);

 private:
  scoped_refptr<ConfigFileWatcherImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(ConfigFileWatcher);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_CONFIG_WATCHER_H_
