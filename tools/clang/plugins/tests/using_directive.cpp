// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "using_directive.h"

int main() {
  // Don't warn here. We use this pattern in unit tests and it's probably
  // harmless. Headers included in multiple translation units could be harmful
  // though.
  using namespace std;
  vector<int> a;
  a.push_back(1);
  return 0;
}
