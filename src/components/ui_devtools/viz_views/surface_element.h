// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIZ_VIEWS_SURFACE_ELEMENT_H_
#define COMPONENTS_UI_DEVTOOLS_VIZ_VIEWS_SURFACE_ELEMENT_H_

#include "base/macros.h"
#include "components/ui_devtools/ui_element.h"
#include "components/viz/common/surfaces/surface_id.h"

namespace viz {
class FrameSinkManagerImpl;
}

namespace ui_devtools {

// A type of UIElement. Each SurfaceElement is corresponding to a SurfaceId.
class SurfaceElement : public UIElement {
 public:
  SurfaceElement(const viz::SurfaceId& surface_id,
                 viz::FrameSinkManagerImpl* frame_sink_manager,
                 UIElementDelegate* ui_element_delegate,
                 UIElement* parent,
                 bool is_detached);
  ~SurfaceElement() override;

  // UIElement:
  std::vector<std::pair<std::string, std::string>> GetCustomProperties()
      const override;
  void GetBounds(gfx::Rect* bounds) const override;
  void SetBounds(const gfx::Rect& bounds) override;
  void GetVisible(bool* visible) const override;
  void SetVisible(bool visible) override;
  std::unique_ptr<protocol::Array<std::string>> GetAttributes() const override;
  std::pair<gfx::NativeWindow, gfx::Rect> GetNodeWindowAndBounds()
      const override;

  static const viz::SurfaceId& From(const UIElement* element);

 private:
  const viz::SurfaceId surface_id_;
  viz::FrameSinkManagerImpl* const frame_sink_manager_;
  gfx::Rect surface_bounds_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceElement);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIZ_VIEWS_SURFACE_ELEMENT_H_