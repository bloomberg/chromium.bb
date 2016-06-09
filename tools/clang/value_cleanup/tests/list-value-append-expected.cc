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
  list.AppendBoolean(1 == 0);
  list.AppendBoolean(true);
  list.AppendInteger(static_cast<unsigned char>(1.0));
  list.AppendDouble(double{3});
  list.AppendString("abc");

  list.Append(ReturnsUniquePtr());
  Thing thing;
  list.Append(thing.ToValue());
  std::unique_ptr<base::Value> unique_ptr_var;
  list.Append(std::move(unique_ptr_var));
}
