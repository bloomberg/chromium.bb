// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/connectivity_diagnostics_dialog.h"

#include <string>

#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

// Scale factor for size of the connectivity diagnostics dialog, based on
// display size.
const float kConnectivityDiagnosticsDialogScale = .8;

}  // namespace

namespace chromeos {

// static
void ConnectivityDiagnosticsDialog::ShowDialog() {
  ConnectivityDiagnosticsDialog* dialog = new ConnectivityDiagnosticsDialog();
  dialog->ShowSystemDialog();
}

ConnectivityDiagnosticsDialog::ConnectivityDiagnosticsDialog()
    : SystemWebDialogDelegate(GURL(kChromeUIConnectivityDiagnosticsUrl),
                              /*title=*/std::u16string()) {}

ConnectivityDiagnosticsDialog::~ConnectivityDiagnosticsDialog() = default;

void ConnectivityDiagnosticsDialog::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  *size =
      gfx::Size(display.size().width() * kConnectivityDiagnosticsDialogScale,
                display.size().height() * kConnectivityDiagnosticsDialogScale);
}

}  // namespace chromeos
