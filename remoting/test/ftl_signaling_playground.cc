// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "remoting/signaling/ftl_client.h"

using namespace remoting;

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);

  FtlClient client;
  ftl::GetICEServerResponse response;
  grpc::Status status = client.GetIceServer(&response);

  if (!status.ok()) {
    LOG(ERROR) << "RPC failed. Code=" << status.error_code() << ", "
               << "Message=" << status.error_message();
    if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
      VLOG(0)
          << "Set the GRPC_DEFAULT_SSL_ROOTS_FILE_PATH environment variable "
          << "to third_party/grpc/src/etc/roots.pem if gRPC cannot locate the "
          << "root certificates.";
    }
    return 1;
  }

  VLOG(0) << "Ice transport policy: "
          << response.ice_config().ice_transport_policy();
  for (const ftl::ICEServerList& server : response.ice_config().ice_servers()) {
    VLOG(0) << "ICE server:";
    VLOG(0) << "  hostname=" << server.hostname();
    VLOG(0) << "  username=" << server.username();
    VLOG(0) << "  credential=" << server.credential();
    VLOG(0) << "  max_rate_kbps=" << server.max_rate_kbps();
    for (const std::string& url : server.urls()) {
      VLOG(0) << "  url=" << url;
    }
  }
  return 0;
}
