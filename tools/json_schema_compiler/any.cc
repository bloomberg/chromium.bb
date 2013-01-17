// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/json_schema_compiler/any.h"

#include "base/logging.h"
#include "base/values.h"

namespace json_schema_compiler {
namespace any {

Any::Any() {}

Any::Any(scoped_ptr<base::Value> from_value)
    : value_(from_value.Pass()) {
}

Any::~Any() {}

void Any::Init(const base::Value& from_value) {
  CHECK(!value_.get());
  value_.reset(from_value.DeepCopy());
}

const base::Value& Any::value() const {
  CHECK(value_.get());
  return *value_;
}

}  // namespace any
}  // namespace json_schema_compiler
