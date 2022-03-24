// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/share/fake_share_history.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace sharing {

FakeShareHistory::FakeShareHistory() = default;
FakeShareHistory::~FakeShareHistory() = default;

void FakeShareHistory::AddShareEntry(const std::string& component_name) {
  NOTIMPLEMENTED();
}

void FakeShareHistory::GetFlatShareHistory(GetFlatHistoryCallback callback,
                                           int window) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), history_));
}

}  // namespace sharing
