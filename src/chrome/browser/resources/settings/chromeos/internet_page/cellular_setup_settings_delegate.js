// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CellularSetupDelegate} from 'chrome://resources/cr_components/chromeos/cellular_setup/cellular_setup_delegate.m.js';

/** @implements {CellularSetupDelegate} */
export class CellularSetupSettingsDelegate {
  /** @override */
  shouldShowPageTitle() {
    return false;
  }

  /** @override */
  shouldShowCancelButton() {
    return true;
  }
}
