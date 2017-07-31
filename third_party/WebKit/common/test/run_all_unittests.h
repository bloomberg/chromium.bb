// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef run_all_unittests_h
#define run_all_unittests_h

#include "base/test/launcher/unit_test_launcher.h"

base::RunTestSuiteCallback GetLaunchCallback(int argc, char** argv);

#endif  // run_all_unittests_h
