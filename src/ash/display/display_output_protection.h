// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_OUTPUT_PROTECTION_H_
#define ASH_DISPLAY_DISPLAY_OUTPUT_PROTECTION_H_

#include <stdint.h>

#include <memory>

#include "ash/public/interfaces/display_output_protection.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace display {
class DisplayConfigurator;
}

namespace ash {

// DisplayOutputProtection provides the necessary functionality to configure
// output protection.
class DisplayOutputProtection : public mojom::DisplayOutputProtection {
 public:
  explicit DisplayOutputProtection(
      display::DisplayConfigurator* display_configurator);
  ~DisplayOutputProtection() override;

  void BindRequest(mojom::DisplayOutputProtectionRequest request);

  // mojom::DisplayOutputProtection:
  void QueryContentProtectionStatus(
      int64_t display_id,
      QueryContentProtectionStatusCallback callback) override;
  void SetContentProtection(int64_t display_id,
                            uint32_t desired_method_mask,
                            SetContentProtectionCallback callback) override;

 private:
  class BindingContext;

  display::DisplayConfigurator* const display_configurator_;

  mojo::BindingSetBase<mojom::DisplayOutputProtection,
                       mojo::Binding<mojom::DisplayOutputProtection>,
                       std::unique_ptr<BindingContext>>
      bindings_;

  DISALLOW_COPY_AND_ASSIGN(DisplayOutputProtection);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_OUTPUT_PROTECTION_H_
