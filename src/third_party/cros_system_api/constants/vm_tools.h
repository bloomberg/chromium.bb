// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_CONSTANTS_VM_TOOLS_H_
#define SYSTEM_API_CONSTANTS_VM_TOOLS_H_

namespace vm_tools {

constexpr int kMaitredPort = 8888;
constexpr int kGarconPort = 8889;
constexpr int kTremplinPort = 8890;
constexpr int kVshPort = 9001;

constexpr int kDefaultStartupListenerPort = 7777;
constexpr int kTremplinListenerPort = 7778;
constexpr int kCrashListenerPort = 7779;

// All ports above this value are reserved for seneschal servers.
constexpr uint32_t kFirstSeneschalServerPort = 16384;

// Name of the user that runs unstrusted operating systems on Chrome OS.
constexpr char kCrosVmUser[] = "crosvm";

}  // namespace vm_tools

#endif  // SYSTEM_API_CONSTANTS_VM_TOOLS_H_
