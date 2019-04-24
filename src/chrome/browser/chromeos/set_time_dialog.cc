// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/set_time_dialog.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/string16.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {

namespace {

const int kDefaultWidth = 490;
const int kDefaultHeight = 235;

}  // namespace

// static
void SetTimeDialog::ShowDialog(gfx::NativeWindow parent) {
  base::RecordAction(base::UserMetricsAction("Options_SetTimeDialog_Show"));
  auto* dialog = new SetTimeDialog();
  dialog->ShowSystemDialog(parent);
}

SetTimeDialog::SetTimeDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUISetTimeURL),
                              base::string16() /* title */) {}

SetTimeDialog::~SetTimeDialog() = default;

void SetTimeDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

}  // namespace chromeos
