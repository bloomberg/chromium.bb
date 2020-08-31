// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_SETTINGS_H_
#define CC_TREES_LAYER_TREE_SETTINGS_H_

#include <stddef.h>

#include <vector>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/tiles/tile_manager_settings.h"
#include "cc/trees/managed_memory_policy.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class CC_EXPORT LayerTreeSettings {
 public:
  LayerTreeSettings();
  LayerTreeSettings(const LayerTreeSettings& other);
  virtual ~LayerTreeSettings();

  SchedulerSettings ToSchedulerSettings() const;
  TileManagerSettings ToTileManagerSettings() const;

  viz::ResourceSettings resource_settings;
  bool single_thread_proxy_scheduler = true;
  bool main_frame_before_activation_enabled = false;
  bool using_synchronous_renderer_compositor = false;
  bool enable_early_damage_check = false;
  // When |enable_early_damage_check| is true, the early damage check is
  // performed if one of the last |damaged_frame_limit| frames had no damage.
  int damaged_frame_limit = 3;
  bool enable_impl_latency_recovery = true;
  bool enable_main_latency_recovery = true;
  bool can_use_lcd_text = true;
  bool gpu_rasterization_disabled = false;
  int gpu_rasterization_msaa_sample_count = -1;
  float gpu_rasterization_skewport_target_time_in_seconds = 0.2f;
  bool create_low_res_tiling = false;
  bool use_stream_video_draw_quad = false;

  enum ScrollbarAnimator {
    NO_ANIMATOR,
    ANDROID_OVERLAY,
    AURA_OVERLAY,
  };
  ScrollbarAnimator scrollbar_animator = NO_ANIMATOR;
  base::TimeDelta scrollbar_fade_delay;
  base::TimeDelta scrollbar_fade_duration;
  base::TimeDelta scrollbar_thinning_duration;
  bool scrollbar_flash_after_any_scroll_update = false;
  bool scrollbar_flash_when_mouse_enter = false;
  SkColor solid_color_scrollbar_color = SK_ColorWHITE;
  base::TimeDelta scroll_animation_duration_for_testing;
  bool timeout_and_draw_when_animation_checkerboards = true;
  bool layers_always_allowed_lcd_text = false;
  float minimum_contents_scale = 0.0625f;
  float low_res_contents_scale_factor = 0.25f;
  float top_controls_show_threshold = 0.5f;
  float top_controls_hide_threshold = 0.5f;
  gfx::Size default_tile_size;
  gfx::Size max_untiled_layer_size;
  // If set, indicates the largest tile size we will use for GPU Raster. If not
  // set, no limit is enforced.
  gfx::Size max_gpu_raster_tile_size;
  // Even for really wide viewports, at some point GPU raster should use
  // less than 4 tiles to fill the viewport. This is set to 256 as a
  // sane minimum for now, but we might want to tune this for low-end.
  int min_height_for_gpu_raster_tile = 256;
  gfx::Size minimum_occlusion_tracking_size;
  // 3000 pixels should give sufficient area for prepainting.
  // Note this value is specified with an ideal contents scale in mind. That
  // is, the ideal tiling would use this value as the padding.
  // TODO(vmpstr): Figure out a better number that doesn't depend on scale.
  int tiling_interest_area_padding = 3000;
  float skewport_target_time_in_seconds = 1.0f;
  int skewport_extrapolation_limit_in_screen_pixels = 2000;
  size_t max_memory_for_prepaint_percentage = 100;
  bool use_zero_copy = false;
  bool use_partial_raster = false;
  bool enable_elastic_overscroll = false;
  bool ignore_root_layer_flings = false;
  size_t scheduled_raster_task_limit = 32;
  bool use_occlusion_for_tile_prioritization = false;
  bool use_layer_lists = false;
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;
  ManagedMemoryPolicy memory_policy;
  size_t decoded_image_working_set_budget_bytes = 128 * 1024 * 1024;
  int max_preraster_distance_in_screen_pixels = 1000;
  bool use_rgba_4444 = false;
  bool unpremultiply_and_dither_low_bit_depth_tiles = false;

  // If set to true, the compositor may selectively defer image decodes to the
  // Image Decode Service and raster tiles without images until the decode is
  // ready.
  bool enable_checker_imaging = false;

  // When content needs a wide color gamut, raster in wide if available.
  // But when the content is sRGB, some situations prefer to raster in
  // wide while others prefer to raster in sRGB.
  bool prefer_raster_in_srgb = false;

  // The minimum size of an image we should considering decoding using the
  // deferred path.
  size_t min_image_bytes_to_checker = 1 * 1024 * 1024;  // 1MB.

  // Disables checkering of images when not using gpu rasterization.
  bool only_checker_images_with_gpu_raster = false;

  LayerTreeDebugState initial_debug_state;

  // Indicates the case when a sub-frame gets its own LayerTree because it's
  // rendered in a different process from its ancestor frames.
  bool is_layer_tree_for_subframe = false;

  // Determines whether we disallow non-exact matches when finding resources
  // in ResourcePool. Only used for layout or pixel tests, as non-deterministic
  // resource sizes can lead to floating point error and noise in these tests.
  bool disallow_non_exact_resource_reuse = false;

  // Whether the Scheduler should wait for all pipeline stages before attempting
  // to draw. If |true|, they will block indefinitely until all stages have
  // completed the current BeginFrame before triggering their own BeginFrame
  // deadlines.
  bool wait_for_all_pipeline_stages_before_draw = false;

  // Determines whether the zoom needs to be applied to the device scale factor.
  bool use_zoom_for_dsf = false;

  // Determines whether mouse interactions on composited scrollbars are handled
  // on the compositor thread.
  bool compositor_threaded_scrollbar_scrolling = true;

  // If enabled, the scroll deltas will be a percentage of the target scroller.
  bool percent_based_scrolling = false;

  // Determines whether animated scrolling is supported. If true, and the
  // incoming gesture scroll is of a type that would normally be animated (e.g.
  // coarse granularity scrolls like those coming from an external mouse wheel),
  // the scroll will be performed smoothly using the animation system rather
  // than instantly.
  bool enable_smooth_scroll = false;

  // Whether layer tree commits should be made directly to the active
  // tree on the impl thread. If |false| LayerTreeHostImpl creates a
  // pending layer tree and produces that as the 'sync tree' with
  // which LayerTreeHost synchronizes. If |true| LayerTreeHostImpl
  // produces the active tree as its 'sync tree'.
  bool commit_to_active_tree = true;

  // Whether image animations can be reset to the beginning to avoid skipping
  // many frames.
  bool enable_image_animation_resync = true;

  // Whether to use edge anti-aliasing for all layer types that supports it.
  bool enable_edge_anti_aliasing = true;

  // Whether SetViewportRectAndScale should update the painted scale factor or
  // the device scale factor.
  bool use_painted_device_scale_factor = false;

  // When true, LayerTreeHostImplClient will be posting a task to call
  // DidReceiveCompositorFrameAck, used by the Compositor but not the
  // LayerTreeView.
  bool send_compositor_frame_ack = true;

  // When false, scroll deltas accumulated on the impl thread are rounded to
  // integer values when sent to Blink on commit. This flag should eventually
  // go away and CC should send Blink fractional values:
  // https://crbug.com/414283.
  bool commit_fractional_scroll_deltas = false;

  // When false, we do not check for occlusion and all quads are drawn.
  // Defaults to true.
  bool enable_occlusion = true;

  // Whether experimental de-jelly effect is allowed.
  bool allow_de_jelly_effect = false;

#if DCHECK_IS_ON()
  // Whether to check if any double blur exists.
  bool log_on_ui_double_background_blur = false;
#endif
};

class CC_EXPORT LayerListSettings : public LayerTreeSettings {
 public:
  LayerListSettings() { use_layer_lists = true; }
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_SETTINGS_H_
