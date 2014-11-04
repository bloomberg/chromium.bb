// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/device_util_linux.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"

namespace ui {

bool IsTouchscreenInternal(const base::FilePath& path) {
  std::string event_node = path.BaseName().value();
  if (event_node.empty() || !StartsWithASCII(event_node, "event", false))
    return false;

  // Extract id "XXX" from "eventXXX"
  std::string event_node_id = event_node.substr(5);

  // I2C input device registers its dev input node at
  // /sys/bus/i2c/devices/*/input/inputXXX/eventXXX
  base::FileEnumerator i2c_enum(
      base::FilePath(FILE_PATH_LITERAL("/sys/bus/i2c/devices/")),
      false,
      base::FileEnumerator::DIRECTORIES);
  for (base::FilePath i2c_name = i2c_enum.Next(); !i2c_name.empty();
       i2c_name = i2c_enum.Next()) {
    base::FileEnumerator input_enum(
        i2c_name.Append(FILE_PATH_LITERAL("input")),
        false,
        base::FileEnumerator::DIRECTORIES,
        FILE_PATH_LITERAL("input*"));
    for (base::FilePath input = input_enum.Next(); !input.empty();
         input = input_enum.Next()) {
      if (input.BaseName().value().substr(5) == event_node_id)
        return true;
    }
  }

  return false;
}

}  // namespace
