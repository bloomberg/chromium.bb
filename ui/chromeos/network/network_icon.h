// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_ICON_H_
#define UI_CHROMEOS_NETWORK_NETWORK_ICON_H_

#include <string>

#include "base/strings/string16.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {
class NetworkState;
}

namespace ui {
namespace network_icon {

class AnimationObserver;

// Type of icon which dictates color theme and VPN badging
enum IconType {
  ICON_TYPE_TRAY,  // light icons with VPN badges
  ICON_TYPE_DEFAULT_VIEW,  // dark icons with VPN badges
  ICON_TYPE_LIST,  // dark icons without VPN badges
};

// Gets the image for the network associated with |service_path|. |network| must
// not be NULL. |icon_type| determines the color theme and whether or not to
// show the VPN badge. This caches badged icons per network per |icon_type|.
UI_CHROMEOS_EXPORT gfx::ImageSkia GetImageForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type);

// Similar to GetImageForNetwork but returns the cached image url based on
// |scale_factor| instead.
UI_CHROMEOS_EXPORT std::string GetImageUrlForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type,
    float scale_factor);

// Gets the fulls strength image for a connected network type.
UI_CHROMEOS_EXPORT gfx::ImageSkia GetImageForConnectedNetwork(
    IconType icon_type,
    const std::string& network_type);

// Gets the image for a connecting network type.
UI_CHROMEOS_EXPORT gfx::ImageSkia GetImageForConnectingNetwork(
    IconType icon_type,
    const std::string& network_type);

// Gets the image for a disconnected network type.
UI_CHROMEOS_EXPORT gfx::ImageSkia GetImageForDisconnectedNetwork(
    IconType icon_type,
    const std::string& network_type);

// Returns the label for |network| based on |icon_type|. |network| can be NULL.
UI_CHROMEOS_EXPORT base::string16 GetLabelForNetwork(
    const chromeos::NetworkState* network,
    IconType icon_type);

// Updates and returns the appropriate message id if the cellular network
// is uninitialized.
UI_CHROMEOS_EXPORT int GetCellularUninitializedMsg();

// Gets the correct icon and label for |icon_type|. Also sets |animating|
// based on whether or not the icon is animating (i.e. connecting).
UI_CHROMEOS_EXPORT void GetDefaultNetworkImageAndLabel(IconType icon_type,
                                                       gfx::ImageSkia* image,
                                                       base::string16* label,
                                                       bool* animating);

// Called when the list of networks changes. Retreives the list of networks
// from the global NetworkStateHandler instance and removes cached entries
// that are no longer in the list.
UI_CHROMEOS_EXPORT void PurgeNetworkIconCache();

}  // namespace network_icon
}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_ICON_H_
