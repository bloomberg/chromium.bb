// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/test_suite.h"

int main(int argc, char** argv) {
  return ui::test::EventsTestSuite(argc, argv).Run();
}
