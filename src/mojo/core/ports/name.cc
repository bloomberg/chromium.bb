// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/ports/name.h"

namespace mojo {
namespace core {
namespace ports {

const PortName kInvalidPortName = {0, 0};

const NodeName kInvalidNodeName = {0, 0};

std::ostream& operator<<(std::ostream& stream, const Name& name) {
  std::ios::fmtflags flags(stream.flags());
  stream << std::hex << std::uppercase << name.v1;
  if (name.v2 != 0)
    stream << '.' << name.v2;
  stream.flags(flags);
  return stream;
}

}  // namespace ports
}  // namespace core
}  // namespace mojo
