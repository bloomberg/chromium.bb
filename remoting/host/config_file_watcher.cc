// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/config_file_watcher.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path_watcher.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/timer.h"

namespace remoting {

// The name of the command-line switch used to specify the host configuration
// file to use.
const char kHostConfigSwitchName[] = "host-config";

const FilePath::CharType kDefaultHostConfigFile[] =
    FILE_PATH_LITERAL("host.json");

class ConfigFileWatcherImpl
    : public base::RefCountedThreadSafe<ConfigFileWatcherImpl> {
 public:
  // Creates a configuration file watcher that lives on the |io_task_runner|
  // thread but posts config file updates on on |main_task_runner|.
  ConfigFileWatcherImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      ConfigFileWatcher::Delegate* delegate);

  // Starts watching |config_path|.
  void Watch(const FilePath& config_path);

  // Stops watching the configuration file.
  void StopWatching();

 private:
  friend class base::RefCountedThreadSafe<ConfigFileWatcherImpl>;
  virtual ~ConfigFileWatcherImpl();

  void FinishStopping();

  // Called every time the host configuration file is updated.
  void OnConfigUpdated(const FilePath& path, bool error);

  // Reads the configuration file and passes it to the delegate.
  void ReloadConfig();

  std::string config_;
  FilePath config_path_;

  scoped_ptr<base::DelayTimer<ConfigFileWatcherImpl> > config_updated_timer_;

  // Monitors the host configuration file.
  scoped_ptr<base::files::FilePathWatcher> config_watcher_;

  base::WeakPtrFactory<ConfigFileWatcher::Delegate> delegate_weak_factory_;
  base::WeakPtr<ConfigFileWatcher::Delegate> delegate_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ConfigFileWatcherImpl);
};

ConfigFileWatcher::Delegate::~Delegate() {
}

ConfigFileWatcher::ConfigFileWatcher(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    Delegate* delegate)
    : impl_(new ConfigFileWatcherImpl(main_task_runner,
                                      io_task_runner, delegate)) {
}

ConfigFileWatcher::~ConfigFileWatcher() {
  impl_->StopWatching();
  impl_ = NULL;
}

void ConfigFileWatcher::Watch(const FilePath& config_path) {
  impl_->Watch(config_path);
}

ConfigFileWatcherImpl::ConfigFileWatcherImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    ConfigFileWatcher::Delegate* delegate)
    : delegate_weak_factory_(delegate),
      delegate_(delegate_weak_factory_.GetWeakPtr()),
      main_task_runner_(main_task_runner),
      io_task_runner_(io_task_runner) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
}

void ConfigFileWatcherImpl::Watch(const FilePath& config_path) {
  if (!io_task_runner_->BelongsToCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ConfigFileWatcherImpl::Watch, this, config_path));
    return;
  }

  DCHECK(config_path_.empty());
  DCHECK(config_updated_timer_.get() == NULL);
  DCHECK(config_watcher_.get() == NULL);

  // Create the timer that will be used for delayed-reading the configuration
  // file.
  config_updated_timer_.reset(new base::DelayTimer<ConfigFileWatcherImpl>(
      FROM_HERE, base::TimeDelta::FromSeconds(2), this,
      &ConfigFileWatcherImpl::ReloadConfig));

  // Start watching the configuration file.
  config_watcher_.reset(new base::files::FilePathWatcher());
  config_path_ = config_path;
  if (!config_watcher_->Watch(
      config_path_,
      base::Bind(&ConfigFileWatcherImpl::OnConfigUpdated, this))) {
    LOG(ERROR) << "Couldn't watch file '" << config_path_.value() << "'";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ConfigFileWatcher::Delegate::OnConfigWatcherError,
                   delegate_));
    return;
  }

  // Force reloading of the configuration file at least once.
  ReloadConfig();
}

void ConfigFileWatcherImpl::StopWatching() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());

  delegate_weak_factory_.InvalidateWeakPtrs();
  io_task_runner_->PostTask(
      FROM_HERE, base::Bind(&ConfigFileWatcherImpl::FinishStopping, this));
}

ConfigFileWatcherImpl::~ConfigFileWatcherImpl() {
  DCHECK(config_updated_timer_.get() == NULL);
  DCHECK(config_watcher_.get() == NULL);
}

void ConfigFileWatcherImpl::FinishStopping() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  config_updated_timer_.reset(NULL);
  config_watcher_.reset(NULL);
}

void ConfigFileWatcherImpl::OnConfigUpdated(const FilePath& path, bool error) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Call ReloadConfig() after a short delay, so that we will not try to read
  // the updated configuration file before it has been completely written.
  // If the writer moves the new configuration file into place atomically,
  // this delay may not be necessary.
  if (!error && config_path_ == path)
    config_updated_timer_->Reset();
}

void ConfigFileWatcherImpl::ReloadConfig() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  std::string config;
  if (!file_util::ReadFileToString(config_path_, &config)) {
    LOG(ERROR) << "Failed to read '" << config_path_.value() << "'";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ConfigFileWatcher::Delegate::OnConfigWatcherError,
                   delegate_));
    return;
  }

  // Post an updated configuration only if it has actually changed.
  if (config_ != config) {
    config_ = config;
    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ConfigFileWatcher::Delegate::OnConfigUpdated, delegate_,
                   config_));
  }
}

}  // namespace remoting
