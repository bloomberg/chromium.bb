// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_NETWORK_VIEW_H_
#define ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_NETWORK_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ui/views/view.h"

namespace ash {

// Fake implementation of NetworkDetailedNetworkView.
class ASH_EXPORT FakeNetworkDetailedNetworkView
    : public NetworkDetailedNetworkView,
      public views::View {
 public:
  explicit FakeNetworkDetailedNetworkView(Delegate* delegate);
  FakeNetworkDetailedNetworkView(const FakeNetworkDetailedNetworkView&) =
      delete;
  FakeNetworkDetailedNetworkView& operator=(
      const FakeNetworkDetailedNetworkView&) = delete;
  ~FakeNetworkDetailedNetworkView() override;

 private:
  // NetworkDetailedNetworkView:
  views::View* GetAsView() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_FAKE_NETWORK_DETAILED_NETWORK_VIEW_H_
