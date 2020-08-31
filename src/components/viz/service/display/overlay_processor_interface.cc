// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_interface.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"

#if defined(OS_MACOSX)
#include "components/viz/service/display/overlay_processor_mac.h"
#elif defined(OS_WIN)
#include "components/viz/service/display/overlay_processor_win.h"
#elif defined(OS_ANDROID)
#include "components/viz/service/display/overlay_processor_android.h"
#include "components/viz/service/display/overlay_processor_surface_control.h"
#elif defined(USE_OZONE)
#include "components/viz/service/display/overlay_processor_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#else
#include "components/viz/service/display/overlay_processor_stub.h"
#endif

namespace viz {
namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnderlayDamage {
  kZeroDamageRect,
  kNonOccludingDamageOnly,
  kOccludingDamageOnly,
  kOccludingAndNonOccludingDamages,
  kMaxValue = kOccludingAndNonOccludingDamages,
};
}  // namespace

// Record UMA histograms for overlays
// 1. Underlay vs. Overlay
// 2. Full screen mode vs. Non Full screen (Windows) mode
// 3. Overlay zero damage rect vs. non zero damage rect
// 4. Underlay zero damage rect, non-zero damage rect with non-occluding damage
//   only, non-zero damage rect with occluding damage, and non-zero damage rect
//   with both damages

// static
void OverlayProcessorInterface::RecordOverlayDamageRectHistograms(
    bool is_overlay,
    bool has_occluding_surface_damage,
    bool zero_damage_rect,
    bool occluding_damage_equal_to_damage_rect) {
  if (is_overlay) {
    UMA_HISTOGRAM_BOOLEAN("Viz.DisplayCompositor.RootDamageRect.Overlay",
                          !zero_damage_rect);
  } else {  // underlay
    UnderlayDamage underlay_damage = UnderlayDamage::kZeroDamageRect;
    if (zero_damage_rect) {
      underlay_damage = UnderlayDamage::kZeroDamageRect;
    } else {
      if (has_occluding_surface_damage) {
        if (occluding_damage_equal_to_damage_rect) {
          underlay_damage = UnderlayDamage::kOccludingDamageOnly;
        } else {
          underlay_damage = UnderlayDamage::kOccludingAndNonOccludingDamages;
        }
      } else {
        underlay_damage = UnderlayDamage::kNonOccludingDamageOnly;
      }
    }
    UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.RootDamageRect.Underlay",
                              underlay_damage);
  }
}

std::unique_ptr<OverlayProcessorInterface>
OverlayProcessorInterface::CreateOverlayProcessor(
    gpu::SurfaceHandle surface_handle,
    const OutputSurface::Capabilities& capabilities,
    const RendererSettings& renderer_settings,
    gpu::SharedImageManager* shared_image_manager,
    gpu::MemoryTracker* memory_tracker,
    scoped_refptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler,
    gpu::SharedImageInterface* shared_image_interface) {
#if defined(OS_MACOSX)
  bool could_overlay = surface_handle != gpu::kNullSurfaceHandle;
  could_overlay &= capabilities.supports_surfaceless;
  bool enable_ca_overlay = could_overlay && renderer_settings.allow_overlays;

  return base::WrapUnique(
      new OverlayProcessorMac(could_overlay, enable_ca_overlay));
#elif defined(OS_WIN)
  bool enable_dc_overlay = surface_handle != gpu::kNullSurfaceHandle;
  enable_dc_overlay &= !capabilities.supports_surfaceless;
  enable_dc_overlay &= capabilities.supports_dc_layers;
  return base::WrapUnique(new OverlayProcessorWin(
      enable_dc_overlay,
      std::make_unique<DCLayerOverlayProcessor>(renderer_settings)));
#elif defined(USE_OZONE)
  bool overlay_enabled = surface_handle != gpu::kNullSurfaceHandle;
  overlay_enabled &= !renderer_settings.overlay_strategies.empty();
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates;
  if (overlay_enabled) {
    auto* overlay_manager =
        ui::OzonePlatform::GetInstance()->GetOverlayManager();
    overlay_candidates =
        overlay_manager->CreateOverlayCandidates(surface_handle);
  }

  if (overlay_enabled && features::ShouldUseRealBuffersForPageFlipTest()) {
    CHECK(shared_image_interface);
  } else {
    shared_image_interface = nullptr;
  }

  return std::make_unique<OverlayProcessorOzone>(
      overlay_enabled, std::move(overlay_candidates),
      std::move(renderer_settings.overlay_strategies), shared_image_interface);
#elif defined(OS_ANDROID)
  bool overlay_enabled = surface_handle != gpu::kNullSurfaceHandle;
  if (capabilities.supports_surfaceless) {
    // This is for Android SurfaceControl case.
    return std::make_unique<OverlayProcessorSurfaceControl>(overlay_enabled);
  } else {
    // When SurfaceControl is enabled, any resource backed by
    // an AHardwareBuffer can be marked as an overlay candidate but it requires
    // that we use a SurfaceControl backed GLSurface. If we're creating a
    // native window backed GLSurface, the overlay processing code will
    // incorrectly assume these resources can be overlaid. So we disable all
    // overlay processing for this OutputSurface.
    overlay_enabled &= !capabilities.android_surface_control_feature_enabled;
    return std::make_unique<OverlayProcessorAndroid>(
        shared_image_manager, memory_tracker, gpu_task_scheduler,
        overlay_enabled);
  }
#else  // Default
  return std::make_unique<OverlayProcessorStub>();
#endif
}

bool OverlayProcessorInterface::DisableSplittingQuads() const {
  return false;
}

OverlayProcessorInterface::OutputSurfaceOverlayPlane
OverlayProcessorInterface::ProcessOutputSurfaceAsOverlay(
    const gfx::Size& viewport_size,
    const gfx::BufferFormat& buffer_format,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    const gpu::Mailbox& mailbox) {
  OutputSurfaceOverlayPlane overlay_plane;
  overlay_plane.transform = gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
  overlay_plane.resource_size = viewport_size;
  overlay_plane.format = buffer_format;
  overlay_plane.color_space = color_space;
  overlay_plane.enable_blending = has_alpha;
  overlay_plane.mailbox = mailbox;

  // Adjust transformation and display_rect based on display rotation.
  overlay_plane.display_rect =
      gfx::RectF(viewport_size.width(), viewport_size.height());

#if defined(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)
  // On Chromecast, always use RGBA as the scanout format for the primary plane.
  overlay_plane.enable_blending = true;
#endif
  return overlay_plane;
}

void OverlayProcessorInterface::ScheduleOverlays(
    DisplayResourceProvider* display_resource_provider) {}

void OverlayProcessorInterface::OverlayPresentationComplete() {}

}  // namespace viz
