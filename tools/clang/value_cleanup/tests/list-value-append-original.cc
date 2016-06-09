// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"

#define true true

std::unique_ptr<base::Value> ReturnsUniquePtr() {
  return nullptr;
}

struct Thing {
  std::unique_ptr<base::Value> ToValue() { return nullptr; }
};

void F() {
  base::ListValue list;
  list.Append(new base::FundamentalValue(1 == 0));
  list.Append(new base::FundamentalValue(true));
  list.Append(new base::FundamentalValue(static_cast<unsigned char>(1.0)));
  list.Append(new base::FundamentalValue(double{3}));
  list.Append(new base::StringValue("abc"));

  list.Append(ReturnsUniquePtr().release());
  Thing thing;
  list.Append(thing.ToValue().release());
  std::unique_ptr<base::Value> unique_ptr_var;
  list.Append(unique_ptr_var.release());
}
