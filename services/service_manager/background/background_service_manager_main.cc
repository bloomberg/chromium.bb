// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background/background_service_manager_main.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/process/launch.h"
#include "services/service_manager/runner/common/switches.h"
#include "services/service_manager/runner/init.h"

int main(int argc, char** argv) {
  return MasterProcessMain(argc, argv);
}
