// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/data_decoder_service.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/data_decoder/image_decoder_impl.h"
#include "services/data_decoder/public/interfaces/image_decoder.mojom.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "services/service_manager/public/cpp/service_context.h"

namespace data_decoder {

namespace {

void OnConnectionLost(std::unique_ptr<service_manager::ServiceContextRef> ref) {
  // No-op. This merely takes ownership of |ref| so it can be destroyed when
  // this function is invoked.
}

void OnImageDecoderRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    mojom::ImageDecoderRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<ImageDecoderImpl>(ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

DataDecoderService::DataDecoderService() : weak_factory_(this) {}

DataDecoderService::~DataDecoderService() = default;

// static
std::unique_ptr<service_manager::Service> DataDecoderService::Create() {
  return base::MakeUnique<DataDecoderService>();
}

void DataDecoderService::OnStart() {
  ref_factory_.reset(new service_manager::ServiceContextRefFactory(base::Bind(
      &DataDecoderService::MaybeRequestQuitDelayed, base::Unretained(this))));
}

bool DataDecoderService::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  // Add a reference to the service and tie it to the lifetime of the
  // InterfaceRegistry's connection.
  std::unique_ptr<service_manager::ServiceContextRef> connection_ref =
      ref_factory_->CreateRef();
  registry->AddConnectionLostClosure(
      base::Bind(&OnConnectionLost, base::Passed(&connection_ref)));
  registry->AddInterface(
      base::Bind(&OnImageDecoderRequest, ref_factory_.get()));
  return true;
}

void DataDecoderService::MaybeRequestQuitDelayed() {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&DataDecoderService::MaybeRequestQuit,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(5));
}

void DataDecoderService::MaybeRequestQuit() {
  DCHECK(ref_factory_);
  if (ref_factory_->HasNoRefs())
    context()->RequestQuit();
}

}  // namespace data_decoder
