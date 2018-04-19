// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/mojo_net_log.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/values.h"
#include "net/log/file_net_log_observer.h"
#include "services/network/public/cpp/network_switches.h"

namespace network {

MojoNetLog::MojoNetLog() {}

MojoNetLog::~MojoNetLog() {
  if (file_net_log_observer_)
    file_net_log_observer_->StopObserving(nullptr /*polled_data*/,
                                          base::OnceClosure());
}

void MojoNetLog::ProcessCommandLine(const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switches::kLogNetLog))
    return;

  base::FilePath log_path =
      command_line.GetSwitchValuePath(switches::kLogNetLog);

  // TODO(eroman): Should get capture mode from the command line.
  net::NetLogCaptureMode capture_mode =
      net::NetLogCaptureMode::IncludeCookiesAndCredentials();

  file_net_log_observer_ = net::FileNetLogObserver::CreateUnbounded(
      log_path, nullptr /* constants */);
  file_net_log_observer_->StartObserving(this, capture_mode);
}

}  // namespace network
