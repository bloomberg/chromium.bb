// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/test/app_list_test_suite.h"

int main(int argc, char** argv) {
  return app_list::test::AppListTestSuite(argc, argv).Run();
}
