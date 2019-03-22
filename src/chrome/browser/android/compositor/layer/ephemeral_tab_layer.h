// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_EPHEMERAL_TAB_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_EPHEMERAL_TAB_LAYER_H_

#include <memory>

#include "chrome/browser/android/compositor/layer/overlay_panel_layer.h"

namespace cc {
class Layer;
class NinePatchLayer;
}  // namespace cc

namespace ui {
class ResourceManager;
}

namespace android {
class EphemeralTabLayer : public OverlayPanelLayer {
 public:
  static scoped_refptr<EphemeralTabLayer> Create(
      ui::ResourceManager* resource_manager);
  void SetProperties(int progress_bar_background_resource_id,
                     int progress_bar_resource_id,
                     float dp_to_px,
                     const scoped_refptr<cc::Layer>& content_layer,
                     float panel_x,
                     float panel_y,
                     float panel_width,
                     float panel_height,
                     float bar_margin_side,
                     float bar_height,
                     float text_opacity,
                     bool bar_border_visible,
                     float bar_border_height,
                     bool bar_shadow_visible,
                     float bar_shadow_opacity,
                     bool progress_bar_visible,
                     float progress_bar_height,
                     float progress_bar_opacity,
                     int progress_bar_completion);

 protected:
  explicit EphemeralTabLayer(ui::ResourceManager* resource_manager);
  ~EphemeralTabLayer() override;

 private:
  scoped_refptr<cc::NinePatchLayer> progress_bar_;
  scoped_refptr<cc::NinePatchLayer> progress_bar_background_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_EPHEMERAL_TAB_LAYER_H_
