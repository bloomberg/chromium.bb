// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/test_server.h"

#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"

namespace {

// Helper class used to detect and kill orphaned python test server processes.
// Checks if the command line of a process contains |path_string| (the path
// from which the test server was launched) and |port_string| (the port used by
// the test server), and if the parent pid of the process is 1 (indicating that
// it is an orphaned process).
class OrphanedTestServerFilter : public base::ProcessFilter {
 public:
  OrphanedTestServerFilter(
      const std::string& path_string, const std::string& port_string)
      : path_string_(path_string),
        port_string_(port_string) {}

  virtual bool Includes(const base::ProcessEntry& entry) const {
    if (entry.parent_pid() != 1)
      return false;
    bool found_path_string = false;
    bool found_port_string = false;
    for (std::vector<std::string>::const_iterator it =
         entry.cmd_line_args().begin();
         it != entry.cmd_line_args().end();
         ++it) {
      if (it->find(path_string_) != std::string::npos)
        found_path_string = true;
      if (it->find(port_string_) != std::string::npos)
        found_port_string = true;
    }
    return found_path_string && found_port_string;
  }

 private:
  std::string path_string_;
  std::string port_string_;
  DISALLOW_COPY_AND_ASSIGN(OrphanedTestServerFilter);
};

}  // namespace

namespace net {
bool TestServer::LaunchPython(const FilePath& testserver_path) {
  std::vector<std::string> command_line;
  command_line.push_back("python");
  command_line.push_back(testserver_path.value());
  command_line.push_back("--port=" + base::IntToString(host_port_pair_.port()));
  command_line.push_back("--data-dir=" + document_root_.value());

  if (type_ == TYPE_FTP)
    command_line.push_back("-f");

  FilePath certificate_path(GetCertificatePath());
  if (!certificate_path.value().empty()) {
    if (!file_util::PathExists(certificate_path)) {
      LOG(ERROR) << "Certificate path " << certificate_path.value()
                 << " doesn't exist. Can't launch https server.";
      return false;
    }
    command_line.push_back("--https=" + certificate_path.value());
  }

  if (type_ == TYPE_HTTPS_CLIENT_AUTH)
    command_line.push_back("--ssl-client-auth");

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    PLOG(ERROR) << "Could not create pipe.";
    return false;
  }

  // Save the read half. The write half is sent to the child.
  child_fd_ = pipefd[0];
  child_fd_closer_.reset(&child_fd_);
  file_util::ScopedFD write_closer(&pipefd[1]);
  base::file_handle_mapping_vector map_write_fd;
  map_write_fd.push_back(std::make_pair(pipefd[1], pipefd[1]));

  command_line.push_back("--startup-pipe=" + base::IntToString(pipefd[1]));

  // Try to kill any orphaned testserver processes that may be running.
  OrphanedTestServerFilter filter(testserver_path.value(),
                                  base::IntToString(host_port_pair_.port()));
  if (!base::KillProcesses(L"python", -1, &filter)) {
    LOG(WARNING) << "Failed to clean up older orphaned testserver instances.";
  }

  // Launch a new testserver process.
  if (!base::LaunchApp(command_line, map_write_fd, false, &process_handle_)) {
    LOG(ERROR) << "Failed to launch " << command_line[0] << " ...";
    return false;
  }

  return true;
}

bool TestServer::WaitToStart() {
  char buf[8];
  ssize_t n = HANDLE_EINTR(read(child_fd_, buf, sizeof(buf)));
  // We don't need the FD anymore.
  child_fd_closer_.reset(NULL);
  return n > 0;
}

bool TestServer::CheckCATrusted() {
  return true;
}

}  // namespace net
