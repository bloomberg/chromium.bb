// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/navigation_entry.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/public/headless_browser.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

namespace {

const char kUseLocalHostForDevToolsHttpServer[] = "localhost";
const int kBackLog = 10;

class TCPEndpointServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit TCPEndpointServerSocketFactory(const net::HostPortPair& endpoint)
      : endpoint_(endpoint) {
    DCHECK(!endpoint_.IsEmpty());
    if (!endpoint.host().empty() &&
        endpoint.host() != kUseLocalHostForDevToolsHttpServer) {
      net::IPAddress ip;
      DCHECK(ip.AssignFromIPLiteral(endpoint.host()));
    }
  }

 private:
  // This function, and the logic below that uses it, is copied from
  // chrome/browser/devtools/remote_debugging_server.cc
  std::unique_ptr<net::ServerSocket> CreateLocalHostServerSocket(int port) {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort("127.0.0.1", port, kBackLog) ==
        net::OK)
      return socket;
    if (socket->ListenWithAddressAndPort("::1", port, kBackLog) == net::OK)
      return socket;
    return std::unique_ptr<net::ServerSocket>();
  }

  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (endpoint_.host() == kUseLocalHostForDevToolsHttpServer)
      return CreateLocalHostServerSocket(endpoint_.port());
    if (socket->ListenWithAddressAndPort(endpoint_.host(), endpoint_.port(),
                                         kBackLog) == net::OK)
      return socket;
    return std::unique_ptr<net::ServerSocket>();
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  net::HostPortPair endpoint_;

  DISALLOW_COPY_AND_ASSIGN(TCPEndpointServerSocketFactory);
};

#if defined(OS_POSIX)
class TCPAdoptServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  // Construct a factory to use an already-open, already-listening socket.
  explicit TCPAdoptServerSocketFactory(const size_t socket_fd)
      : socket_fd_(socket_fd) {}

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::TCPServerSocket> tsock(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (tsock->AdoptSocket(socket_fd_) != net::OK) {
      LOG(ERROR) << "Failed to adopt open socket";
      return std::unique_ptr<net::ServerSocket>();
    }
    // Note that we assume that the socket is already listening, so unlike
    // TCPEndpointServerSocketFactory, we don't call Listen.
    return std::unique_ptr<net::ServerSocket>(std::move(tsock));
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  size_t socket_fd_;

  DISALLOW_COPY_AND_ASSIGN(TCPAdoptServerSocketFactory);
};
#else   // defined(OS_POSIX)

// Placeholder class to use when a socket_fd is passed in on non-Posix.
class DummyTCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit DummyTCPServerSocketFactory() {}

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    return nullptr;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  DISALLOW_COPY_AND_ASSIGN(DummyTCPServerSocketFactory);
};
#endif  // defined(OS_POSIX)
}  // namespace

void StartLocalDevToolsHttpHandler(HeadlessBrowser::Options* options) {
  if (options->devtools_pipe_enabled)
    content::DevToolsAgentHost::StartRemoteDebuggingPipeHandler();
  if (options->devtools_endpoint.IsEmpty())
    return;

  std::unique_ptr<content::DevToolsSocketFactory> socket_factory;
  const net::HostPortPair& endpoint = options->devtools_endpoint;
  socket_factory.reset(new TCPEndpointServerSocketFactory(endpoint));

  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(socket_factory),
      options->user_data_dir,  // TODO(altimin): Figure a proper value for this.
      base::FilePath());
}

void StopLocalDevToolsHttpHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
  content::DevToolsAgentHost::StopRemoteDebuggingPipeHandler();
}

}  // namespace headless
