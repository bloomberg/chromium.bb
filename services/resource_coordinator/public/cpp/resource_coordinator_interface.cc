// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"

#include <utility>

#include "mojo/public/cpp/bindings/binding.h"
#include "services/resource_coordinator/public/interfaces/coordination_unit_provider.mojom.h"
#include "services/resource_coordinator/public/interfaces/service_constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

void OnConnectionError() {
  DCHECK(false);
}

}  // namespace

namespace resource_coordinator {

ResourceCoordinatorInterface::ResourceCoordinatorInterface(
    service_manager::Connector* connector,
    const CoordinationUnitType& type,
    const std::string& id)
    : weak_ptr_factory_(this) {
  CoordinationUnitID new_cu_id(type, id);
  ConnectToService(connector, new_cu_id);
}

ResourceCoordinatorInterface::ResourceCoordinatorInterface(
    service_manager::Connector* connector,
    const CoordinationUnitType& type)
    : ResourceCoordinatorInterface(connector, type, std::string()) {}

ResourceCoordinatorInterface::ResourceCoordinatorInterface(
    service_manager::Connector* connector,
    const CoordinationUnitType& type,
    uint64_t id)
    : weak_ptr_factory_(this) {
  CoordinationUnitID new_cu_id(type, id);
  ConnectToService(connector, new_cu_id);
}

ResourceCoordinatorInterface::~ResourceCoordinatorInterface() = default;

void ResourceCoordinatorInterface::ConnectToService(
    service_manager::Connector* connector,
    const CoordinationUnitID& cu_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!connector)
    return;
  cu_id_ = cu_id;
  mojom::CoordinationUnitProviderPtr provider;
  connector->BindInterface(mojom::kServiceName, mojo::MakeRequest(&provider));

  provider->CreateCoordinationUnit(mojo::MakeRequest(&service_), cu_id);

  service_.set_connection_error_handler(base::Bind(&OnConnectionError));
}

void ResourceCoordinatorInterface::SendEvent(const mojom::Event event) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!service_)
    return;

  service_->SendEvent(event);
}

void ResourceCoordinatorInterface::SetProperty(
    mojom::PropertyType property_type,
    int64_t value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!service_)
    return;
  service_->SetProperty(property_type, value);
}

void ResourceCoordinatorInterface::AddBinding(
    mojom::CoordinationUnitRequest request) {
  if (!service_)
    return;
  service_->AddBinding(std::move(request));
}

void ResourceCoordinatorInterface::AddChild(
    const ResourceCoordinatorInterface& child) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!service_)
    return;
  // We could keep the ID around ourselves, but this hop ensures that the child
  // has been created on the service-side.
  child.service()->GetID(base::Bind(&ResourceCoordinatorInterface::AddChildByID,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ResourceCoordinatorInterface::RemoveChild(
    const ResourceCoordinatorInterface& child) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!service_)
    return;
  // We could keep the ID around ourselves, but this hop ensures that the child
  // has been created on the service-side.
  child.service()->GetID(
      base::Bind(&ResourceCoordinatorInterface::RemoveChildByID,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ResourceCoordinatorInterface::AddChildByID(
    const CoordinationUnitID& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  service_->AddChild(child_id);
}

void ResourceCoordinatorInterface::RemoveChildByID(
    const CoordinationUnitID& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  service_->RemoveChild(child_id);
}

}  // namespace resource_coordinator
