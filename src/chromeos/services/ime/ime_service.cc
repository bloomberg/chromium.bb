// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/ime_service.h"

#include "base/bind.h"
#include "build/buildflag.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
#include "chromeos/services/ime/decoder/decoder_engine.h"
#endif

namespace chromeos {
namespace ime {

ImeService::ImeService(
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)
    : service_binding_(this, std::move(receiver)) {}

ImeService::~ImeService() = default;

void ImeService::OnStart() {
  binders_.Add(base::BindRepeating(&ImeService::AddInputEngineManagerReceiver,
                                   base::Unretained(this)));

  manager_receivers_.set_disconnect_handler(base::BindRepeating(
      &ImeService::OnConnectionLost, base::Unretained(this)));

#if BUILDFLAG(ENABLE_CROS_IME_DECODER)
  input_engine_ = std::make_unique<DecoderEngine>(
      service_binding_.GetConnector(), base::SequencedTaskRunnerHandle::Get());
#else
  input_engine_ = std::make_unique<InputEngine>();
#endif
}

void ImeService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  binders_.TryBind(interface_name, &receiver_pipe);
}

void ImeService::ConnectToImeEngine(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
    mojo::PendingRemote<mojom::InputChannel> from_engine,
    const std::vector<uint8_t>& extra,
    ConnectToImeEngineCallback callback) {
  DCHECK(input_engine_);
  bool bound = input_engine_->BindRequest(
      ime_spec, std::move(to_engine_request), std::move(from_engine), extra);
  std::move(callback).Run(bound);
}

void ImeService::AddInputEngineManagerReceiver(
    mojo::PendingReceiver<mojom::InputEngineManager> receiver) {
  manager_receivers_.Add(this, std::move(receiver));
  // TODO(https://crbug.com/837156): Reset the cleanup timer.
}

void ImeService::OnConnectionLost() {
  if (manager_receivers_.empty()) {
    service_binding_.RequestClose();
    // TODO(https://crbug.com/837156): Set a timer to start a cleanup.
  }
}

}  // namespace ime
}  // namespace chromeos
