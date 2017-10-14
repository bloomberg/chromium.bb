// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VALUES_H_
#define VALUES_H_

namespace base {

class Value {
 public:
  enum class Type {
    NONE = 0,
    BOOLEAN,
    INTEGER,
    DOUBLE,
    STRING,
    BINARY,
    DICTIONARY,
    LIST
  };

  // Returns the type of the value stored by the current Value object.
  Type GetType() const { return type_; }  // DEPRECATED, use type().
  Type type() const { return type_; }

 private:
  Type type_ = Type::NONE;
};

}  // namespace base

#endif  // VALUES_H_
