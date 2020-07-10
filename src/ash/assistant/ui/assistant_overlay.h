// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_OVERLAY_H_
#define ASH_ASSISTANT_UI_ASSISTANT_OVERLAY_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

// AssistantOverlays are children of AssistantContainerView's client view that
// behave as pseudo-children of AssistantContainerView's own child views. This
// allows children of AssistantContainerView to pseudo-parent views that paint
// to a layer that is higher up in the layer tree than themselves. In the case
// of AssistantMainView, an overlay is used to parent in-Assistant notification
// views that need to be drawn over top of Assistant cards.
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantOverlay : public views::View {
 public:
  // Defines parameters for how an overlay should be laid out.
  struct LayoutParams {
    enum Gravity {
      kUnspecified = 0,
      kBottom = 1 << 0,
      kCenterHorizontal = 1 << 1,
    };
    int gravity = Gravity::kUnspecified;
    gfx::Insets margins;
  };

  AssistantOverlay() = default;
  ~AssistantOverlay() override = default;

  // Returns parameters for how an overlay should be laid out.
  virtual LayoutParams GetLayoutParams() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantOverlay);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_OVERLAY_H_
