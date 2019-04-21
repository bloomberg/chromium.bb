// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/util_win_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/services/util_win/public/mojom/util_win.mojom.h"
#include "chrome/services/util_win/util_win_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

void OnUtilWinRequest(service_manager::ServiceKeepalive* keepalive,
                      chrome::mojom::UtilWinRequest request) {
  mojo::MakeStrongBinding(std::make_unique<UtilWinImpl>(keepalive->CreateRef()),
                          std::move(request));
}

}  // namespace

UtilWinService::UtilWinService(service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

UtilWinService::~UtilWinService() = default;

void UtilWinService::OnStart() {
  registry_.AddInterface(
      base::BindRepeating(&OnUtilWinRequest, &service_keepalive_));
}

void UtilWinService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
