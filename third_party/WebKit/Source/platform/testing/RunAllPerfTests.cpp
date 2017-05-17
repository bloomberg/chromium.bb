// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/testing/BlinkPerfTestSuite.h"

int main(int argc, char** argv) {
  return blink::BlinkPerfTestSuite(argc, argv).Run();
}
