// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_CONFIG_H_
#define TOOLS_GN_CONFIG_H_

#include "base/macros.h"
#include "tools/gn/config_values.h"
#include "tools/gn/item.h"

// Represents a named config in the dependency graph.
class Config : public Item {
 public:
  Config(const Settings* settings, const Label& label);
  ~Config() override;

  Config* AsConfig() override;
  const Config* AsConfig() const override;

  ConfigValues& config_values() { return config_values_; }
  const ConfigValues& config_values() const { return config_values_; }

 private:
  ConfigValues config_values_;

  DISALLOW_COPY_AND_ASSIGN(Config);
};

#endif  // TOOLS_GN_CONFIG_H_
