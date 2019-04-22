// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/output_protection_controller_ash.h"
#include "ui/display/manager/display_configurator.h"

#include "ash/shell.h"  // mash-ok

namespace {

display::DisplayConfigurator* configurator() {
  return ash::Shell::Get()->display_configurator();
}

}  // namespace

namespace chromeos {

OutputProtectionControllerAsh::OutputProtectionControllerAsh()
    : client_id_(configurator()->RegisterContentProtectionClient()) {}

OutputProtectionControllerAsh::~OutputProtectionControllerAsh() {
  DCHECK(thread_checker_.CalledOnValidThread());
  configurator()->UnregisterContentProtectionClient(client_id_);
}

void OutputProtectionControllerAsh::QueryStatus(
    int64_t display_id,
    const OutputProtectionDelegate::QueryStatusCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  configurator()->QueryContentProtection(client_id_, display_id, callback);
}

void OutputProtectionControllerAsh::SetProtection(
    int64_t display_id,
    uint32_t desired_method_mask,
    const OutputProtectionDelegate::SetProtectionCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  configurator()->ApplyContentProtection(client_id_, display_id,
                                         desired_method_mask, callback);
}

}  // namespace chromeos
