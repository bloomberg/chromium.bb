// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_LIST_DELEGATE_H_
#define UI_CHROMEOS_NETWORK_NETWORK_LIST_DELEGATE_H_

#include "ui/chromeos/ui_chromeos_export.h"

namespace chromeos {
class NetworkState;
class NetworkTypePattern;
}

namespace views {
class Label;
class View;
}

namespace ui {

struct NetworkInfo;

class UI_CHROMEOS_EXPORT NetworkListDelegate {
 public:
  virtual ~NetworkListDelegate() {}

  // Creates and returns a View with the information in |info|.
  virtual views::View* CreateViewForNetwork(const NetworkInfo& info) = 0;

  // Returns true if |view| is currently under the cursor. Note that |view| is
  // guaranteed to be a View returned from |CreateViewForNetwork()|.
  virtual bool IsViewHovered(views::View* view) = 0;

  // Returns the type of network this list should use.
  virtual chromeos::NetworkTypePattern GetNetworkTypePattern() const = 0;

  // Updates |view| with the information in |info|. Note that |view| is
  // guaranteed to be a View returned from |CreateViewForNetwork()|.
  virtual void UpdateViewForNetwork(views::View* view,
                                    const NetworkInfo& info) = 0;

  // Creates a Label to be displayed in the list to present some information
  // (e.g. unavailability of network etc.).
  virtual views::Label* CreateInfoLabel() = 0;

  virtual void RelayoutScrollList() = 0;
};

}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_LIST_DELEGATE_H_
