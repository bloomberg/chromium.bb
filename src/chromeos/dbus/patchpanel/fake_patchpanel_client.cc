// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/patchpanel/fake_patchpanel_client.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

namespace {

FakePatchPanelClient* g_instance = nullptr;

}  // namespace

// static
FakePatchPanelClient* FakePatchPanelClient::Get() {
  return g_instance;
}

FakePatchPanelClient::FakePatchPanelClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakePatchPanelClient::~FakePatchPanelClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakePatchPanelClient::GetDevices(GetDevicesCallback callback) {}

}  // namespace chromeos
