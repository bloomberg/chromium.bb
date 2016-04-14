// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHELL_TESTS_UTIL_H_
#define SERVICES_SHELL_TESTS_UTIL_H_

#include <memory>
#include <string>

namespace base {
class Process;
}

namespace shell {
class Connection;
class Connector;
class Identity;

namespace test {

// Starts the process @ |target_exe_name| and connects to it as |target| using
// |connector|, returning the connection & the process.
// This blocks until the connection is established/rejected by the shell.
std::unique_ptr<Connection> LaunchAndConnectToProcess(
    const std::string& target_exe_name,
    const Identity target,
    shell::Connector* connector,
    base::Process* process);

}  // namespace test
}  // namespace shell

#endif  // SERVICES_SHELL_TESTS_UTIL_H_
