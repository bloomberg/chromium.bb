// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_ZAURA_SHELL_H_
#define COMPONENTS_EXO_WAYLAND_ZAURA_SHELL_H_

#include <stdint.h>

#include "components/exo/surface.h"
#include "components/exo/surface_observer.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/wm/public/activation_change_observer.h"

struct wl_client;
struct wl_resource;

namespace exo {

class ShellSurfaceBase;

namespace wayland {

constexpr uint32_t kZAuraShellVersion = 28;

// Adds bindings to the Aura Shell. Normally this implies Ash on ChromeOS
// builds. On non-ChromeOS builds the protocol provides access to Aura windowing
// system.
void bind_aura_shell(wl_client* client,
                     void* data,
                     uint32_t version,
                     uint32_t id);

class AuraSurface : public SurfaceObserver,
                    public ::wm::ActivationChangeObserver {
 public:
  AuraSurface(Surface* surface, wl_resource* resource);

  AuraSurface(const AuraSurface&) = delete;
  AuraSurface& operator=(const AuraSurface&) = delete;

  ~AuraSurface() override;

  void SetFrame(SurfaceFrameType type);
  void SetServerStartResize();
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);
  void SetParent(AuraSurface* parent, const gfx::Point& position);
  void SetStartupId(const char* startup_id);
  void SetApplicationId(const char* application_id);
  void SetClientSurfaceId(const char* client_surface_id);
  void SetOcclusionTracking(bool tracking);
  void Activate();
  void DrawAttention();
  void SetFullscreenMode(uint32_t mode);
  void IntentToSnap(uint32_t snap_direction);
  void SetSnapPrimary();
  void SetSnapSecondary();
  void UnsetSnap();
  void SetWindowSessionId(int32_t window_session_id);
  void SetCanGoBack();
  void UnsetCanGoBack();
  void SetPip();
  void UnsetPip();
  void SetAspectRatio(const gfx::SizeF& aspect_ratio);
  void MoveToDesk(int desk_index);
  void SetInitialWorkspace(const char* initial_workspace);
  void Pin(bool trusted);
  void Unpin();
  void SetOrientationLock(uint32_t orientation_lock);

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;
  void OnWindowOcclusionChanged(Surface* surface) override;
  void OnFrameLockingChanged(Surface* surface, bool lock) override;
  void OnDeskChanged(Surface* surface, int state) override;

  // Overridden from ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gaining_active,
                          aura::Window* losing_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}

 protected:
  virtual void SendOcclusionFraction(float occlusion_fraction);
  virtual void SendOcclusionState(
      const aura::Window::OcclusionState occlusion_state);

 private:
  Surface* surface_;
  wl_resource* const resource_;

  void ComputeAndSendOcclusion(
      const aura::Window::OcclusionState occlusion_state,
      const SkRegion& occluded_region);
};

// Provides an implementation for top level operations on the shell.
class AuraToplevel {
 public:
  AuraToplevel(ShellSurfaceBase* shell_surface);
  AuraToplevel(const AuraToplevel&) = delete;
  AuraToplevel& operator=(const AuraToplevel&) = delete;
  ~AuraToplevel();

  void SetOrientationLock(uint32_t lock_type);
  void SetClientSubmitsSurfacesInPixelCoordinates(bool enable);

 private:
  ShellSurfaceBase* shell_surface_;
};

class AuraPopup {
 public:
  AuraPopup(ShellSurfaceBase* shell_surface);
  AuraPopup(const AuraPopup&) = delete;
  AuraPopup& operator=(const AuraPopup&) = delete;
  ~AuraPopup();

  void SetClientSubmitsSurfacesInPixelCoordinates(bool enable);

 private:
  ShellSurfaceBase* shell_surface_;
};

}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_ZAURA_SHELL_H_
