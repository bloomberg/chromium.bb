// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_suite.h"

class V2TestSuite : public base::TestSuite {
 public:
  V2TestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  virtual void Initialize() {
    base::TestSuite::Initialize();
  }
};

int main(int argc, char **argv) {
  return V2TestSuite(argc, argv).Run();
}
