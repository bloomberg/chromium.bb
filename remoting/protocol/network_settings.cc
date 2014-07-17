// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/network_settings.h"

#include <limits.h>
#include <stdlib.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace remoting {
namespace protocol {

// static
  bool NetworkSettings::ParsePortRange(const std::string& port_range,
                                       int* out_min_port,
                                       int* out_max_port) {
  size_t separator_index = port_range.find('-');
  if (separator_index == std::string::npos)
    return false;

  std::string min_port_string, max_port_string;
  base::TrimWhitespaceASCII(port_range.substr(0, separator_index),
                            base::TRIM_ALL,
                            &min_port_string);
  base::TrimWhitespaceASCII(port_range.substr(separator_index + 1),
                            base::TRIM_ALL,
                            &max_port_string);

  unsigned min_port, max_port;
  if (!base::StringToUint(min_port_string, &min_port) ||
      !base::StringToUint(max_port_string, &max_port)) {
    return false;
  }

  if (min_port == 0 || min_port > max_port || max_port > USHRT_MAX)
    return false;

  *out_min_port = static_cast<int>(min_port);
  *out_max_port = static_cast<int>(max_port);
  return true;
}

}  // namespace protocol
}  // namespace remoting
