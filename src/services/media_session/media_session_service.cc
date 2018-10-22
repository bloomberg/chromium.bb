// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_session_service.h"

#include "services/service_manager/public/cpp/service_context.h"

namespace media_session {

std::unique_ptr<service_manager::Service> MediaSessionService::Create() {
  return std::make_unique<MediaSessionService>();
}

MediaSessionService::MediaSessionService() = default;

MediaSessionService::~MediaSessionService() = default;

void MediaSessionService::OnStart() {
  DLOG(ERROR) << "start";
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(
      context()->CreateQuitClosure()));
}

void MediaSessionService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace media_session
