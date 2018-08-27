// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_IME_IME_REGISTRAR_IMPL_H_
#define SERVICES_WS_IME_IME_REGISTRAR_IMPL_H_

#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/ws/ime/ime_driver_bridge.h"
#include "services/ws/public/mojom/ime/ime.mojom.h"

namespace ui {

class IMERegistrarImpl : public ws::mojom::IMERegistrar {
 public:
  explicit IMERegistrarImpl(IMEDriverBridge* ime_driver_bridge);
  ~IMERegistrarImpl() override;

  void AddBinding(ws::mojom::IMERegistrarRequest request);

 private:
  // ws::mojom::IMERegistrar:
  void RegisterDriver(ws::mojom::IMEDriverPtr driver) override;

  mojo::BindingSet<ws::mojom::IMERegistrar> bindings_;
  IMEDriverBridge* ime_driver_bridge_;

  DISALLOW_COPY_AND_ASSIGN(IMERegistrarImpl);
};

}  // namespace ui

#endif  // SERVICES_WS_IME_IME_REGISTRAR_IMPL_H_
