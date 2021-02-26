// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_
#define REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/protocol/ice_config_request.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace remoting {

namespace apis {
namespace v1 {
class GetIceConfigResponse;
}  // namespace v1
}  // namespace apis

class ProtobufHttpStatus;

namespace protocol {

// IceConfigRequest that fetches IceConfig from the remoting NetworkTraversal
// service.
class RemotingIceConfigRequest final : public IceConfigRequest {
 public:
  explicit RemotingIceConfigRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~RemotingIceConfigRequest() override;

  // IceConfigRequest implementation.
  void Send(OnIceConfigCallback callback) override;

 private:
  friend class RemotingIceConfigRequestTest;

  void OnResponse(const ProtobufHttpStatus& status,
                  std::unique_ptr<apis::v1::GetIceConfigResponse> response);

  OnIceConfigCallback on_ice_config_callback_;
  ProtobufHttpClient http_client_;

  DISALLOW_COPY_AND_ASSIGN(RemotingIceConfigRequest);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_REMOTING_ICE_CONFIG_REQUEST_H_
