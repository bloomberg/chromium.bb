// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_SERVICE_MANAGEMENT_H_
#define CHROME_COMMON_MAC_SERVICE_MANAGEMENT_H_

#include <string>
#include <vector>

namespace mac {
namespace services {

struct JobOptions {
  JobOptions();
  JobOptions(const JobOptions& other);
  ~JobOptions();

  std::string label;
  // See launchd.plist(5) for details about these two fields. In short:
  //   - executable_path, if non-empty, is the absolute path to the executable
  //     for this job;
  //   - arguments[0] is the absolute *or relative* path to the executable if
  //     executable_path is empty; in either case, all of arguments is passed
  //     directly as argv to the job, meaning arguments[0] becomes argv[0]
  // There are important caveats about the argument vector documented in the
  // launchd.plist(5) man page.
  std::string executable_path;
  std::vector<std::string> arguments;

  // See launchd.plist(5) "Sockets" for details about the meaning of these two
  // fields. The socket_key field corresponds to a top-level key in the socket
  // dictionary; the socket_name field corresponds to a pathname to a Unix
  // domain socket (the SockPathName member in the Sockets dictionary).
  std::string socket_name;
  std::string socket_key;

  // Whether to run this job immediately once it is loaded.
  bool run_at_load;

  // Whether to restart the job whenever it exits successfully. This also
  // implicitly limits the job to interactive sessions only (i.e., the job will
  // not run in system sessions).
  bool auto_launch;
};

bool SubmitJob(const JobOptions& options);
bool RemoveJob(const std::string& label);

}  // namespace services
}  // namespace mac

#endif  // CHROME_COMMON_MAC_SERVICE_MANAGEMENT_H_
