// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_CONFIG_VALUES_EXTRACTORS_H_
#define TOOLS_GN_CONFIG_VALUES_EXTRACTORS_H_

#include <ostream>
#include <string>
#include <vector>

#include "tools/gn/config.h"
#include "tools/gn/config_values.h"
#include "tools/gn/target.h"

template<typename T, class Writer>
inline void ConfigValuesToStream(
    const ConfigValues& values,
    const std::vector<T>& (ConfigValues::* getter)() const,
    const Writer& writer,
    std::ostream& out) {
  const std::vector<T>& v = (values.*getter)();
  for (size_t i = 0; i < v.size(); i++)
    writer(v[i], out);
};

template<typename T, class Writer>
inline void RecursiveTargetConfigToStream(
    const Target* target,
    const std::vector<T>& (ConfigValues::* getter)() const,
    const Writer& writer,
    std::ostream& out) {
  // Write all configs in reverse order (to get oldest first, which will look
  // more normal in the output).
  for (int i = static_cast<int>(target->configs().size() - 1); i >= 0; i--) {
    ConfigValuesToStream(target->configs()[i]->config_values(), getter,
                         writer, out);
  }

  // Last write from the config from the Target itself, if any.
  ConfigValuesToStream(target->config_values(), getter, writer, out);
}

// Writes the values out as strings with no transformation.
void RecursiveTargetConfigStringsToStream(
    const Target* target,
    const std::vector<std::string>& (ConfigValues::* getter)() const,
    std::ostream& out);

#endif  // TOOLS_GN_CONFIG_VALUES_EXTRACTORS_H_
