// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_scan_results_proxy.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"

namespace chrome_cleaner {

EngineScanResultsProxy::EngineScanResultsProxy(
    mojom::EngineScanResultsAssociatedPtr scan_results_ptr,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : scan_results_ptr_(std::move(scan_results_ptr)),
      task_runner_(task_runner) {}

void EngineScanResultsProxy::UnbindScanResultsPtr() {
  scan_results_ptr_.reset();
}

void EngineScanResultsProxy::FoundUwS(UwSId pup_id, const PUPData::PUP& pup) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EngineScanResultsProxy::OnFoundUwS, this, pup_id, pup));
}

void EngineScanResultsProxy::ScanDone(uint32_t result) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EngineScanResultsProxy::OnDone, this, result));
}

EngineScanResultsProxy::~EngineScanResultsProxy() = default;

// Invokes scan_results_ptr_->FoundUwS from the IPC thread.
void EngineScanResultsProxy::OnFoundUwS(UwSId pup_id, const PUPData::PUP& pup) {
  if (!scan_results_ptr_.is_bound()) {
    LOG(ERROR) << "Found UwS reported after the engine was shut down";
    return;
  }
  scan_results_ptr_->FoundUwS(pup_id, pup);
}

// Invokes scan_results_ptr_->Done from the IPC thread.
void EngineScanResultsProxy::OnDone(uint32_t result) {
  if (!scan_results_ptr_.is_bound()) {
    LOG(ERROR) << "Scan result reported after the engine was shut down";
    return;
  }
  scan_results_ptr_->Done(result);
}

}  // namespace chrome_cleaner
