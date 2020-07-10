// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_CLIENTS_BLUR_H_
#define COMPONENTS_EXO_WAYLAND_CLIENTS_BLUR_H_

#include "components/exo/wayland/clients/client_base.h"

class SkImage;

namespace exo {
namespace wayland {
namespace clients {

// Client that can be used to measure the cost of blur filter effects
// across different devices.
class Blur : public wayland::clients::ClientBase {
 public:
  Blur();
  ~Blur() override;

  void Run(double sigma_x,
           double sigma_y,
           double max_sigma,
           bool offscreen,
           int frames);

 private:
  sk_sp<SkImage> grid_image_;

  DISALLOW_COPY_AND_ASSIGN(Blur);
};

}  // namespace clients
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_CLIENTS_BLUR_H_
