// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_FACTORY_H_
#define ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_FACTORY_H_

#include "ash/services/nearby/public/mojom/firewall_hole.mojom.h"

namespace ash {
namespace nearby {

// A simple implementation of sharing::mojom::FirewallHoleFactory used for
// testing.
class FakeFirewallHoleFactory : public sharing::mojom::FirewallHoleFactory {
 public:
  FakeFirewallHoleFactory();
  ~FakeFirewallHoleFactory() override;

  // Immediately invokes |callback| with a fake firewall hole if
  // |should_succeed_| is true and NullRemote if false.
  void OpenFirewallHole(const ash::nearby::TcpServerSocketPort& port,
                        OpenFirewallHoleCallback callback) override;

  bool should_succeed_ = true;
};

}  // namespace nearby
}  // namespace ash

#endif  // ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_FIREWALL_HOLE_FACTORY_H_
