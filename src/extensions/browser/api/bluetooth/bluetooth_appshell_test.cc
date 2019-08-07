// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_apitest.h"

using BluetoothShellApiTest = extensions::ShellApiTest;

IN_PROC_BROWSER_TEST_F(BluetoothShellApiTest, ApiSanityCheck) {
  ASSERT_TRUE(RunAppTest("api_test/bluetooth"));
}
