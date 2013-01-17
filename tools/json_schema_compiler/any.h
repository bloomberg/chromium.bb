// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_JSON_SCHEMA_COMPILER_ANY_H__
#define TOOLS_JSON_SCHEMA_COMPILER_ANY_H__

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class Value;
}

namespace json_schema_compiler {
namespace any {

// Represents an "any" type in JSON schema as a wrapped Value.
class Any {
 public:
  Any();
  explicit Any(scoped_ptr<base::Value> from_value);
  ~Any();

  // Initializes the Value in this Any. Fails if already initialized.
  void Init(const base::Value& from_value);

  // Get the Value from this Any.
  const base::Value& value() const;

 private:
  scoped_ptr<base::Value> value_;

  DISALLOW_COPY_AND_ASSIGN(Any);
};

}  // namespace any
}  // namespace json_schema_compiler

#endif // TOOLS_JSON_SCHEMA_COMPILER_ANY_H__
