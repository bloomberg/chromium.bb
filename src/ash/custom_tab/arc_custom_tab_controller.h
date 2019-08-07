// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_CONTROLLER_H_
#define ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_CONTROLLER_H_

#include "ash/public/interfaces/arc_custom_tab.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {

// Provides the ArcCustomTabController interface to the outside world.
class ArcCustomTabController : public mojom::ArcCustomTabController {
 public:
  ArcCustomTabController();
  ~ArcCustomTabController() override;

  void BindRequest(mojom::ArcCustomTabControllerRequest request);

  // ArcCustomTabController:
  void CreateView(int32_t task_id,
                  int32_t surface_id,
                  int32_t top_margin,
                  CreateViewCallback callback) override;

 private:
  mojo::Binding<mojom::ArcCustomTabController> binding_;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomTabController);
};

}  // namespace ash

#endif  // ASH_CUSTOM_TAB_ARC_CUSTOM_TAB_CONTROLLER_H_
