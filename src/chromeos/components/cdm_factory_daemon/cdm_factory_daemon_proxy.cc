// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy.h"

#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

CdmFactoryDaemonProxy::CdmFactoryDaemonProxy()
    : mojo_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

CdmFactoryDaemonProxy::~CdmFactoryDaemonProxy() = default;

void CdmFactoryDaemonProxy::BindReceiver(
    mojo::PendingReceiver<BrowserCdmFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace chromeos
