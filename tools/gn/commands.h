// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COMMANDS_H_
#define TOOLS_GN_COMMANDS_H_

#include <string>
#include <vector>

// The different commands we have, returns the value we should return from
// main().
int RunDepsCommand(const std::vector<std::string>& args);
int RunDescCommand(const std::vector<std::string>& args);
int RunGenCommand(const std::vector<std::string>& args);
int RunTreeCommand(const std::vector<std::string>& args);

#endif  // TOOLS_GN_COMMANDS_H_
