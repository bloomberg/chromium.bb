// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/certificate_watcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/thread_task_runner_handle.h"

namespace remoting {

// Delay time to restart the host when a change of certificate is detected.
// This is to avoid repeating restarts when continuous writes to the database
// occur.
const int kRestartDelayInSecond = 2;

// Full Path: $HOME/.pki/nssdb
const char kCertDirectoryPath[] = ".pki/nssdb";

CertificateWatcher::CertificateWatcher(
    const base::Closure& restart_action,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : restart_action_(restart_action),
      caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      delay_(base::TimeDelta::FromSeconds(kRestartDelayInSecond)),
      weak_factory_(this) {
  if (!base::PathService::Get(base::DIR_HOME, &cert_watch_path_)) {
    LOG(FATAL) << "Failed to get path of the home directory.";
  }
  cert_watch_path_ = cert_watch_path_.AppendASCII(kCertDirectoryPath);
}

CertificateWatcher::~CertificateWatcher() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (!is_started()) {
    return;
  }
  if (monitor_) {
    monitor_->RemoveStatusObserver(this);
  }
  io_task_runner_->DeleteSoon(FROM_HERE, file_watcher_.release());

  VLOG(1) << "Stopped watching certificate changes.";
}

void CertificateWatcher::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(!cert_watch_path_.empty());

  file_watcher_.reset(new base::FilePathWatcher());
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&base::FilePathWatcher::Watch),
                 base::Unretained(file_watcher_.get()), cert_watch_path_, true,
                 base::Bind(&CertificateWatcher::OnCertDirectoryChanged,
                            caller_task_runner_, weak_factory_.GetWeakPtr())));
  restart_timer_.reset(new base::DelayTimer(FROM_HERE, delay_, this,
                                            &CertificateWatcher::OnTimer));

  VLOG(1) << "Started watching certificate changes.";
}

void CertificateWatcher::SetMonitor(base::WeakPtr<HostStatusMonitor> monitor) {
  DCHECK(is_started());
  if (monitor_) {
    monitor_->RemoveStatusObserver(this);
  }
  monitor->AddStatusObserver(this);
  monitor_ = monitor;
}

void CertificateWatcher::OnClientConnected(const std::string& jid) {
  DCHECK(is_started());
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  inhibit_mode_ = true;
}

void CertificateWatcher::OnClientDisconnected(const std::string& jid) {
  DCHECK(is_started());
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  inhibit_mode_ = false;
  if (restart_pending_) {
    restart_pending_ = false;
    restart_action_.Run();
  }
}

void CertificateWatcher::SetDelayForTests(const base::TimeDelta& delay) {
  DCHECK(!is_started());
  delay_ = delay;
}

void CertificateWatcher::SetWatchPathForTests(
    const base::FilePath& watch_path) {
  DCHECK(!is_started());
  cert_watch_path_ = watch_path;
}

bool CertificateWatcher::is_started() const {
  return file_watcher_ != nullptr;
}

// static
void CertificateWatcher::OnCertDirectoryChanged(
    scoped_refptr<base::SingleThreadTaskRunner> network_task_runner,
    base::WeakPtr<CertificateWatcher> watcher_,
    const base::FilePath& path,
    bool error) {
  network_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&CertificateWatcher::DirectoryChanged, watcher_, path, error));
}

void CertificateWatcher::DirectoryChanged(const base::FilePath& path,
                                          bool error) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(path == cert_watch_path_);

  if (error) {
    LOG(FATAL) << "Error occurs when watching changes of file "
               << cert_watch_path_.MaybeAsASCII();
  }

  restart_timer_->Reset();
}

void CertificateWatcher::OnTimer() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  if (inhibit_mode_) {
    restart_pending_ = true;
    return;
  }

  VLOG(1) << "Certificate was updated. Calling restart...";
  restart_action_.Run();
}

}  // namespace remoting
