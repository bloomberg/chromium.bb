// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/spawned_test_server/remote_test_server.h"

#include <stdint.h>

#include <limits>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/test/spawned_test_server/remote_test_server_config.h"
#include "net/test/spawned_test_server/spawner_communicator.h"
#include "url/gurl.h"

namespace net {

namespace {

// Please keep it sync with dictionary SERVER_TYPES in testserver.py
std::string GetServerTypeString(BaseTestServer::Type type) {
  switch (type) {
    case BaseTestServer::TYPE_FTP:
      return "ftp";
    case BaseTestServer::TYPE_HTTP:
    case BaseTestServer::TYPE_HTTPS:
      return "http";
    case BaseTestServer::TYPE_WS:
    case BaseTestServer::TYPE_WSS:
      return "ws";
    case BaseTestServer::TYPE_TCP_ECHO:
      return "tcpecho";
    case BaseTestServer::TYPE_UDP_ECHO:
      return "udpecho";
    default:
      NOTREACHED();
  }
  return std::string();
}

}  // namespace

RemoteTestServer::RemoteTestServer(Type type,
                                   const base::FilePath& document_root)
    : BaseTestServer(type) {
  if (!Init(document_root))
    NOTREACHED();
}

RemoteTestServer::RemoteTestServer(Type type,
                                   const SSLOptions& ssl_options,
                                   const base::FilePath& document_root)
    : BaseTestServer(type, ssl_options) {
  if (!Init(document_root))
    NOTREACHED();
}

RemoteTestServer::~RemoteTestServer() {
  Stop();
}

bool RemoteTestServer::Start() {
  if (spawner_communicator_.get())
    return true;

  spawner_communicator_ =
      std::make_unique<SpawnerCommunicator>(RemoteTestServerConfig::Load());

  base::DictionaryValue arguments_dict;
  if (!GenerateArguments(&arguments_dict))
    return false;

  arguments_dict.Set("on-remote-server", std::make_unique<base::Value>());

  // Append the 'server-type' argument which is used by spawner server to
  // pass right server type to Python test server.
  arguments_dict.SetString("server-type", GetServerTypeString(type()));

  // Generate JSON-formatted argument string.
  std::string arguments_string;
  base::JSONWriter::Write(arguments_dict, &arguments_string);
  if (arguments_string.empty())
    return false;

  // Start the Python test server on the remote machine.
  std::string server_data;
  if (!spawner_communicator_->StartServer(arguments_string, &server_data))
    return false;

  // Parse server_data.
  if (server_data.empty() || !ParseServerData(server_data)) {
    LOG(ERROR) << "Could not parse server_data: " << server_data;
    return false;
  }

  return SetupWhenServerStarted();
}

bool RemoteTestServer::StartInBackground() {
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTestServer::BlockUntilStarted() {
  NOTIMPLEMENTED();
  return false;
}

bool RemoteTestServer::Stop() {
  if (!spawner_communicator_)
    return true;

  uint16_t port = GetPort();
  CleanUpWhenStoppingServer();
  bool stopped = spawner_communicator_->StopServer(port);

  if (!stopped)
    LOG(ERROR) << "Failed stopping RemoteTestServer";

  // Explicitly reset |spawner_communicator_| to avoid reusing the stopped one.
  spawner_communicator_.reset();
  return stopped;
}

// On Android, the document root in the device is not the same as the document
// root in the host machine where the test server is launched. So prepend
// DIR_SOURCE_ROOT here to get the actual path of document root on the Android
// device.
base::FilePath RemoteTestServer::GetDocumentRoot() const {
  base::FilePath src_dir;
  PathService::Get(base::DIR_SOURCE_ROOT, &src_dir);
  return src_dir.Append(document_root());
}

bool RemoteTestServer::Init(const base::FilePath& document_root) {
  if (document_root.IsAbsolute())
    return false;

  // Unlike LocalTestServer, RemoteTestServer passes relative paths to the test
  // server. The test server fails on empty strings in some configurations.
  base::FilePath fixed_root = document_root;
  if (fixed_root.empty())
    fixed_root = base::FilePath(base::FilePath::kCurrentDirectory);
  SetResourcePath(fixed_root, base::FilePath()
                                  .AppendASCII("net")
                                  .AppendASCII("data")
                                  .AppendASCII("ssl")
                                  .AppendASCII("certificates"));
  return true;
}

}  // namespace net
