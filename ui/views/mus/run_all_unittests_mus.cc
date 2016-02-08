// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/run_all_unittests.h"

int MasterProcessMain(int argc, char** argv) {
  return views::RunAllUnittests(argc, argv);
}
