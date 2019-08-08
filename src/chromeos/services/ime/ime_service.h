// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_IME_SERVICE_H_
#define CHROMEOS_SERVICES_IME_IME_SERVICE_H_

#include "chromeos/services/ime/input_engine.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_map.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace chromeos {
namespace ime {

class ImeService : public service_manager::Service,
                   public mojom::InputEngineManager {
 public:
  explicit ImeService(
      mojo::PendingReceiver<service_manager::mojom::Service> receiver);
  ~ImeService() override;

 private:
  // service_manager::Service overrides:
  void OnStart() override;
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  // mojom::InputEngineManager overrides:
  void ConnectToImeEngine(
      const std::string& ime_spec,
      mojo::PendingReceiver<mojom::InputChannel> to_engine_request,
      mojo::PendingRemote<mojom::InputChannel> from_engine,
      const std::vector<uint8_t>& extra,
      ConnectToImeEngineCallback callback) override;

  // Adds a mojom::InputEngineManager receiver to this object.
  void AddInputEngineManagerReceiver(
      mojo::PendingReceiver<mojom::InputEngineManager> receiver);

  void OnConnectionLost();

  service_manager::ServiceBinding service_binding_;

  // For the duration of this service lifetime, there should be only one
  // input engine instance.
  std::unique_ptr<InputEngine> input_engine_;

  mojo::ReceiverSet<mojom::InputEngineManager> manager_receivers_;

  service_manager::BinderMap binders_;

  DISALLOW_COPY_AND_ASSIGN(ImeService);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_IME_SERVICE_H_
