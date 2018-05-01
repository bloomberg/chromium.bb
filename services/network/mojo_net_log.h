// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MOJO_NET_LOG_H_
#define SERVICES_NETWORK_MOJO_NET_LOG_H_

#include <memory>

#include "base/macros.h"
#include "net/log/net_log.h"

namespace base {
class CommandLine;
}  // namespace base

namespace net {
class FileNetLogObserver;
}  // namespace net

namespace network {

// NetLog used by NetworkService when it owns the NetLog, rather than when a
// pre-existing one is passed in to its constructor.
//
// Currently only provides --log-net-log support.
class MojoNetLog : public net::NetLog {
 public:
  MojoNetLog();
  ~MojoNetLog() override;

  // If specified by the command line, stream network events (NetLog) to a
  // file on disk. This will last for the duration of the process.
  void ProcessCommandLine(const base::CommandLine& command_line);

 private:
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(MojoNetLog);
};

}  // namespace network

#endif  // SERVICES_NETWORK_MOJO_NET_LOG_H_
