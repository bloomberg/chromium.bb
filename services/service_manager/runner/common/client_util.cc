// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/common/client_util.h"

#include <string>

#include "base/command_line.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/pending_process_connection.h"
#include "services/service_manager/runner/common/switches.h"

namespace service_manager {

mojom::ServicePtr PassServiceRequestOnCommandLine(
    mojo::edk::PendingProcessConnection* connection,
    base::CommandLine* command_line) {
  std::string token;
  mojom::ServicePtr client;
  client.Bind(mojom::ServicePtrInfo(connection->CreateMessagePipe(&token), 0));
  command_line->AppendSwitchASCII(switches::kPrimordialPipeToken, token);
  return client;
}

mojom::ServiceRequest GetServiceRequestFromCommandLine() {
  std::string token =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPrimordialPipeToken);
  mojom::ServiceRequest request;
  if (!token.empty())
    request.Bind(mojo::edk::CreateChildMessagePipe(token));
  return request;
}

bool ServiceManagerIsRemote() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrimordialPipeToken);
}

}  // namespace service_manager
