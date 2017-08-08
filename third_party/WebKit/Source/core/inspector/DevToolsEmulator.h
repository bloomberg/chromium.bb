// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DevToolsEmulator_h
#define DevToolsEmulator_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Optional.h"
#include "public/platform/PointerProperties.h"
#include "public/platform/WebFloatPoint.h"
#include "public/platform/WebViewportStyle.h"
#include "public/web/WebDeviceEmulationParams.h"

namespace blink {

class IntPoint;
class IntRect;
class TransformationMatrix;
class WebInputEvent;
class WebViewBase;

class CORE_EXPORT DevToolsEmulator final
    : public GarbageCollectedFinalized<DevToolsEmulator> {
 public:
  ~DevToolsEmulator();
  static DevToolsEmulator* Create(WebViewBase*);
  DECLARE_TRACE();

  // Settings overrides.
  void SetTextAutosizingEnabled(bool);
  void SetDeviceScaleAdjustment(float);
  void SetPreferCompositingToLCDTextEnabled(bool);
  void SetViewportStyle(WebViewportStyle);
  void SetPluginsEnabled(bool);
  void SetScriptEnabled(bool);
  void SetDoubleTapToZoomEnabled(bool);
  bool DoubleTapToZoomEnabled() const;
  void SetAvailablePointerTypes(int);
  void SetPrimaryPointerType(PointerType);
  void SetAvailableHoverTypes(int);
  void SetPrimaryHoverType(HoverType);
  void SetMainFrameResizesAreOrientationChanges(bool);

  // Emulation.
  void EnableDeviceEmulation(const WebDeviceEmulationParams&);
  void DisableDeviceEmulation();
  // Position is given in CSS pixels, scale relative to a page scale of 1.0.
  void ForceViewport(const WebFloatPoint& position, float scale);
  void ResetViewport();
  bool ResizeIsDeviceSizeChange();
  void SetTouchEventEmulationEnabled(bool);
  bool HandleInputEvent(const WebInputEvent&);
  void SetScriptExecutionDisabled(bool);

  // Notify the DevToolsEmulator about a scroll or scale change of the main
  // frame. Updates the transform for a viewport override.
  void MainFrameScrollOrScaleChanged();

  // Returns a custom visible content rect if a viewport override is active.
  // This ensures that all content inside the forced viewport is painted.
  WTF::Optional<IntRect> VisibleContentRectForPainting() const;

 private:
  explicit DevToolsEmulator(WebViewBase*);

  void EnableMobileEmulation();
  void DisableMobileEmulation();

  // Returns the original device scale factor when overridden by DevTools, or
  // deviceScaleFactor() otherwise.
  float CompositorDeviceScaleFactor() const;

  void ApplyDeviceEmulationTransform(TransformationMatrix*);
  void ApplyViewportOverride(TransformationMatrix*);
  void UpdateRootLayerTransform();

  WebViewBase* web_view_;

  bool device_metrics_enabled_;
  bool emulate_mobile_enabled_;
  WebDeviceEmulationParams emulation_params_;

  struct ViewportOverride {
    WebFloatPoint position;
    double scale;
    bool original_visual_viewport_masking;
  };
  WTF::Optional<ViewportOverride> viewport_override_;

  bool is_overlay_scrollbars_enabled_;
  bool is_orientation_event_enabled_;
  bool is_mobile_layout_theme_enabled_;
  float original_default_minimum_page_scale_factor_;
  float original_default_maximum_page_scale_factor_;
  bool embedder_text_autosizing_enabled_;
  float embedder_device_scale_adjustment_;
  bool embedder_prefer_compositing_to_lcd_text_enabled_;
  WebViewportStyle embedder_viewport_style_;
  bool embedder_plugins_enabled_;
  int embedder_available_pointer_types_;
  PointerType embedder_primary_pointer_type_;
  int embedder_available_hover_types_;
  HoverType embedder_primary_hover_type_;
  bool embedder_main_frame_resizes_are_orientation_changes_;

  bool touch_event_emulation_enabled_;
  bool double_tap_to_zoom_enabled_;
  bool original_device_supports_touch_;
  int original_max_touch_points_;
  std::unique_ptr<IntPoint> last_pinch_anchor_css_;
  std::unique_ptr<IntPoint> last_pinch_anchor_dip_;

  bool embedder_script_enabled_;
  bool script_execution_disabled_;
};

}  // namespace blink

#endif
