// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/coordination_unit/coordination_unit_provider_impl.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "services/resource_coordinator/coordination_unit/frame_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/page_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/process_coordination_unit_impl.h"
#include "services/resource_coordinator/coordination_unit/system_coordination_unit_impl.h"
#include "services/service_manager/public/cpp/bind_source_info.h"

namespace resource_coordinator {

CoordinationUnitProviderImpl::CoordinationUnitProviderImpl(
    service_manager::ServiceKeepalive* service_keepalive,
    CoordinationUnitGraph* coordination_unit_graph)
    : service_keepalive_(service_keepalive),
      coordination_unit_graph_(coordination_unit_graph) {
  DCHECK(service_keepalive_);
  keepalive_ref_ = service_keepalive_->CreateRef();
}

CoordinationUnitProviderImpl::~CoordinationUnitProviderImpl() = default;

void CoordinationUnitProviderImpl::OnConnectionError(
    CoordinationUnitBase* coordination_unit) {
  coordination_unit->Destruct();
}

void CoordinationUnitProviderImpl::CreateFrameCoordinationUnit(
    mojom::FrameCoordinationUnitRequest request,
    const CoordinationUnitID& id) {
  FrameCoordinationUnitImpl* frame_cu =
      coordination_unit_graph_->CreateFrameCoordinationUnit(
          id, service_keepalive_->CreateRef());

  frame_cu->Bind(std::move(request));
  auto& frame_cu_binding = frame_cu->binding();

  frame_cu_binding.set_connection_error_handler(
      base::BindOnce(&CoordinationUnitProviderImpl::OnConnectionError,
                     base::Unretained(this), frame_cu));
}

void CoordinationUnitProviderImpl::CreatePageCoordinationUnit(
    mojom::PageCoordinationUnitRequest request,
    const CoordinationUnitID& id) {
  PageCoordinationUnitImpl* page_cu =
      coordination_unit_graph_->CreatePageCoordinationUnit(
          id, service_keepalive_->CreateRef());

  page_cu->Bind(std::move(request));
  auto& page_cu_binding = page_cu->binding();

  page_cu_binding.set_connection_error_handler(
      base::BindOnce(&CoordinationUnitProviderImpl::OnConnectionError,
                     base::Unretained(this), page_cu));
}

void CoordinationUnitProviderImpl::CreateProcessCoordinationUnit(
    mojom::ProcessCoordinationUnitRequest request,
    const CoordinationUnitID& id) {
  ProcessCoordinationUnitImpl* process_cu =
      coordination_unit_graph_->CreateProcessCoordinationUnit(
          id, service_keepalive_->CreateRef());

  process_cu->Bind(std::move(request));
  auto& process_cu_binding = process_cu->binding();

  process_cu_binding.set_connection_error_handler(
      base::BindOnce(&CoordinationUnitProviderImpl::OnConnectionError,
                     base::Unretained(this), process_cu));
}

void CoordinationUnitProviderImpl::GetSystemCoordinationUnit(
    resource_coordinator::mojom::SystemCoordinationUnitRequest request) {
  // Simply fetch the existing SystemCU and add an additional binding to it.
  coordination_unit_graph_
      ->FindOrCreateSystemCoordinationUnit(service_keepalive_->CreateRef())
      ->AddBinding(std::move(request));
}

void CoordinationUnitProviderImpl::Bind(
    resource_coordinator::mojom::CoordinationUnitProviderRequest request,
    const service_manager::BindSourceInfo& source_info) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace resource_coordinator
