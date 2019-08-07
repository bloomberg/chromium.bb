// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DEVTOOLS_SERVER_H_
#define CHROME_BROWSER_ANDROID_DEVTOOLS_SERVER_H_

#include <memory>
#include <string>

#include "base/macros.h"

// This class controls Developer Tools remote debugging server.
class DevToolsServer {
 public:
  explicit DevToolsServer(const std::string& socket_name_prefix);
  ~DevToolsServer();

  // Opens linux abstract socket to be ready for remote debugging.
  void Start(bool allow_debug_permission);

  // Closes debugging socket, stops debugging.
  void Stop();

  bool IsStarted() const;

 private:
  std::string socket_name_;
  bool is_started_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsServer);
};

#endif  // CHROME_BROWSER_ANDROID_DEVTOOLS_SERVER_H_
