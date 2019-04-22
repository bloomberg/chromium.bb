// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/removable_storage_writer/removable_storage_writer_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "chrome/services/removable_storage_writer/removable_storage_writer.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

void OnRemovableStorageWriterGetterRequest(
    service_manager::ServiceKeepalive* keepalive,
    chrome::mojom::RemovableStorageWriterRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<RemovableStorageWriter>(keepalive->CreateRef()),
      std::move(request));
}

}  // namespace

RemovableStorageWriterService::RemovableStorageWriterService(
    service_manager::mojom::ServiceRequest request)
    : service_binding_(this, std::move(request)),
      service_keepalive_(&service_binding_, base::TimeDelta()) {}

RemovableStorageWriterService::~RemovableStorageWriterService() = default;

void RemovableStorageWriterService::OnStart() {
  registry_.AddInterface(base::BindRepeating(
      &OnRemovableStorageWriterGetterRequest, &service_keepalive_));
}

void RemovableStorageWriterService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}
