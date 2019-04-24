// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_CONFIG_H_
#define CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_CONFIG_H_

#include <string>

#include "base/macros.h"

namespace base {
class CommandLine;
}

// Class used to start a test license server.
class TestLicenseServerConfig {
 public:
  TestLicenseServerConfig() {}
  virtual ~TestLicenseServerConfig() {}

  // Returns a string containing the URL and port the server is listening to.
  // This URL is directly used by the Web app to request a license, example:
  // http://localhost:8888/license_server
  virtual std::string GetServerURL() = 0;

  // Returns true if it successfully sets the command line to run the license
  // server with needed args and switches.
  virtual bool GetServerCommandLine(base::CommandLine* command_line) = 0;

  // Returns true if the server is supported on current platform.
  virtual bool IsPlatformSupported() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLicenseServerConfig);
};

#endif  // CHROME_BROWSER_MEDIA_TEST_LICENSE_SERVER_CONFIG_H_
