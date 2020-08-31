// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_CAST_UI_DEVTOOLS_H_
#define CHROMECAST_BROWSER_DEVTOOLS_CAST_UI_DEVTOOLS_H_

#include <memory>

#include "base/macros.h"

namespace network {
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace ui_devtools {
class UiDevToolsServer;
}  // namespace ui_devtools

namespace chromecast {
namespace shell {

// Container to instantiate a separate devtools server for viewing the hierarchy
// of the Aura UI tree. Instantiated only when USE_AURA is true and
// --enable-ui-devtools is passed.
class CastUIDevTools {
 public:
  explicit CastUIDevTools(network::mojom::NetworkContext* network_context);
  ~CastUIDevTools();

 private:
  std::unique_ptr<ui_devtools::UiDevToolsServer> CreateServer(
      network::mojom::NetworkContext* network_context) const;

  std::unique_ptr<ui_devtools::UiDevToolsServer> devtools_server_;

  DISALLOW_COPY_AND_ASSIGN(CastUIDevTools);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_CAST_UI_DEVTOOLS_H_
