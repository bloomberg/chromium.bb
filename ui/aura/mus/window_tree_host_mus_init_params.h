// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_WINDOW_TREE_HOST_MUS_INIT_PARAMS_H_
#define UI_AURA_MUS_WINDOW_TREE_HOST_MUS_INIT_PARAMS_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cc/surfaces/frame_sink_id.h"
#include "ui/aura/aura_export.h"

namespace aura {

class WindowPortMus;
class WindowTreeClient;

// Used to create a WindowTreeHostMus. The typical case is to use
// CreateInitParamsForTopLevel().
struct AURA_EXPORT WindowTreeHostMusInitParams {
  WindowTreeHostMusInitParams();
  WindowTreeHostMusInitParams(WindowTreeHostMusInitParams&& other);
  ~WindowTreeHostMusInitParams();

  // The WindowTreeClient; must be specified.
  WindowTreeClient* window_tree_client = nullptr;

  // Used to create the Window; must be specified.
  std::unique_ptr<WindowPortMus> window_port;

  // Properties to send to the server as well as to set on the Window.
  std::map<std::string, std::vector<uint8_t>> properties;

  cc::FrameSinkId frame_sink_id;

  // Id of the display the window should be created on.
  int64_t display_id = 0;
};

// Creates a WindowTreeHostMusInitParams that is used when creating a top-level
// window.
AURA_EXPORT WindowTreeHostMusInitParams CreateInitParamsForTopLevel(
    WindowTreeClient* window_tree_client,
    std::map<std::string, std::vector<uint8_t>> properties =
        std::map<std::string, std::vector<uint8_t>>());

}  // namespace aura

#endif  // UI_AURA_MUS_WINDOW_TREE_HOST_MUS_INIT_PARAMS_H_
