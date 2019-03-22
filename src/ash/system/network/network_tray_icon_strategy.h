// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_TRAY_ICON_STRATEGY_H_
#define ASH_SYSTEM_NETWORK_NETWORK_TRAY_ICON_STRATEGY_H_

#include "base/macros.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {
namespace tray {

// Represents an abstract stategy to get the appropriate network icon image to
// render on the system tray. Different strategies are needed for each type of
// network icon we want to show on the tray.
class NetworkTrayIconStrategy {
 public:
  virtual ~NetworkTrayIconStrategy() = default;

  // Returns a network icon image and sets |animating| when a network icon
  // should be displayed on the tray.
  // Returns a null ImageSkia when no network icon should be displyed,
  // |animating| is not modified.
  virtual gfx::ImageSkia GetNetworkIcon(bool* animating) = 0;
};

// Strategy for rendering non-mobile network icons.
class DefaultNetworkTrayIconStrategy : public NetworkTrayIconStrategy {
 public:
  DefaultNetworkTrayIconStrategy() = default;
  ~DefaultNetworkTrayIconStrategy() override = default;

  // NetworkTrayIconStrategy:
  gfx::ImageSkia GetNetworkIcon(bool* animating) override;

  DISALLOW_COPY_AND_ASSIGN(DefaultNetworkTrayIconStrategy);
};

// Strategy for rendering Mobile network icon.
class MobileNetworkTrayIconStrategy : public NetworkTrayIconStrategy {
 public:
  MobileNetworkTrayIconStrategy() = default;
  ~MobileNetworkTrayIconStrategy() override = default;

  // NetworkTrayIconStrategy:
  gfx::ImageSkia GetNetworkIcon(bool* animating) override;

  DISALLOW_COPY_AND_ASSIGN(MobileNetworkTrayIconStrategy);
};

// Strategy for rendering a single unified WiFi and Cellular network icon.
// TODO(tonydeluna): Remove once the _____ is enabled by default.
class SingleNetworkTrayIconStrategy : public NetworkTrayIconStrategy {
 public:
  SingleNetworkTrayIconStrategy() = default;
  ~SingleNetworkTrayIconStrategy() override = default;

  // NetworkTrayIconStrategy:
  gfx::ImageSkia GetNetworkIcon(bool* animating) override;

  DISALLOW_COPY_AND_ASSIGN(SingleNetworkTrayIconStrategy);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_TRAY_ICON_STRATEGY_H_
