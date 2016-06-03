// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"

#define true true

void F() {
  base::ListValue list;
  list.Append(new base::FundamentalValue(1 == 0));
  list.Append(new base::FundamentalValue(true));
  list.Append(new base::FundamentalValue(static_cast<unsigned char>(1.0)));
  list.Append(new base::FundamentalValue(double{3}));
  list.Append(new base::StringValue("abc"));
}
