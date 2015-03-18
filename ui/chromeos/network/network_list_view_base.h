// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_LIST_VIEW_BASE_H_
#define UI_CHROMEOS_NETWORK_NETWORK_LIST_VIEW_BASE_H_

#include <string>

#include "base/macros.h"
#include "ui/chromeos/ui_chromeos_export.h"

namespace views {
class View;
}

namespace ui {

// Base class for a list of available networks (and, in the case of VPNs, the
// list of available VPN providers).
class UI_CHROMEOS_EXPORT NetworkListViewBase {
 public:
  NetworkListViewBase();
  virtual ~NetworkListViewBase();

  void set_container(views::View* container) { container_ = container; }

  // Refreshes the network list.
  virtual void Update() = 0;

  // Checks whether |view| represents a network in the list. If yes, sets
  // |service_path| to the network's service path and returns |true|. Otherwise,
  // leaves |sevice_path| unchanged and returns |false|.
  virtual bool IsNetworkEntry(views::View* view,
                              std::string* service_path) const = 0;

 protected:
  // The container that holds the actual list entries.
  views::View* container_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkListViewBase);
};

}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_LIST_VIEW_BASE_H_
