// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
#define COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_

#include <stdint.h>

#include "base/macros.h"

struct wl_global;

namespace exo {
namespace wayland {

// Class that represent a wayland output. Tied to a specific display ID
// and associated with a global.
class WaylandDisplayOutput {
 public:
  explicit WaylandDisplayOutput(int64_t id);
  ~WaylandDisplayOutput();

  int64_t id() const;
  void set_global(wl_global* global);

 private:
  const int64_t id_;
  wl_global* global_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WaylandDisplayOutput);
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_WAYLAND_DISPLAY_OUTPUT_H_
