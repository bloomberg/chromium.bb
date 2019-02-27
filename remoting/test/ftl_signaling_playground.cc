// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/signaling/ftl_client.h"

namespace remoting {

class FtlSignalingPlayground {
 public:
  FtlSignalingPlayground();
  ~FtlSignalingPlayground();

  void GetIceServer(base::OnceClosure on_done);

 private:
  static void OnGetIceServerResponse(base::OnceClosure on_done,
                                     grpc::Status status,
                                     const ftl::GetICEServerResponse& response);

  FtlClient client_;
  DISALLOW_COPY_AND_ASSIGN(FtlSignalingPlayground);
};

FtlSignalingPlayground::FtlSignalingPlayground() = default;

FtlSignalingPlayground::~FtlSignalingPlayground() = default;

void FtlSignalingPlayground::GetIceServer(base::OnceClosure on_done) {
  client_.GetIceServer(base::BindOnce(
      &FtlSignalingPlayground::OnGetIceServerResponse, std::move(on_done)));
  VLOG(0) << "Running GetIceServer...";
}

// static
void FtlSignalingPlayground::OnGetIceServerResponse(
    base::OnceClosure on_done,
    grpc::Status status,
    const ftl::GetICEServerResponse& response) {
  if (status.ok()) {
    VLOG(0) << "Ice transport policy: "
            << response.ice_config().ice_transport_policy();
    for (const ftl::ICEServerList& server :
         response.ice_config().ice_servers()) {
      VLOG(0) << "ICE server:";
      VLOG(0) << "  hostname=" << server.hostname();
      VLOG(0) << "  username=" << server.username();
      VLOG(0) << "  credential=" << server.credential();
      VLOG(0) << "  max_rate_kbps=" << server.max_rate_kbps();
      for (const std::string& url : server.urls()) {
        VLOG(0) << "  url=" << url;
      }
    }
  } else {
    LOG(ERROR) << "RPC failed. Code=" << status.error_code() << ", "
               << "Message=" << status.error_message();
    if (status.error_code() == grpc::StatusCode::UNAVAILABLE) {
      VLOG(0)
          << "Set the GRPC_DEFAULT_SSL_ROOTS_FILE_PATH environment variable "
          << "to third_party/grpc/src/etc/roots.pem if gRPC cannot locate the "
          << "root certificates.";
    }
  }
  std::move(on_done).Run();
}

}  // namespace remoting

int main(int argc, char const* argv[]) {
  base::AtExitManager exitManager;
  base::CommandLine::Init(argc, argv);

  base::MessageLoopForIO message_loop;

  remoting::FtlSignalingPlayground playground;
  base::RunLoop run_loop;
  playground.GetIceServer(run_loop.QuitClosure());
  run_loop.Run();

  return 0;
}
