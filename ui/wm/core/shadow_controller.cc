// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_controller.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

using std::make_pair;

DECLARE_UI_CLASS_PROPERTY_TYPE(::wm::Shadow*);
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(::wm::Shadow, kShadowLayerKey, nullptr);

namespace wm {

namespace {

constexpr ShadowElevation kInactiveNormalShadowElevation =
    ShadowElevation::MEDIUM;

ShadowElevation GetDefaultShadowElevationForWindow(aura::Window* window) {
  switch (window->type()) {
    case aura::client::WINDOW_TYPE_NORMAL:
    case aura::client::WINDOW_TYPE_PANEL:
      return kInactiveNormalShadowElevation;

    case aura::client::WINDOW_TYPE_MENU:
    case aura::client::WINDOW_TYPE_TOOLTIP:
      return ShadowElevation::SMALL;

    default:
      break;
  }
  return ShadowElevation::NONE;
}

// Returns the ShadowElevation for |window|, converting |DEFAULT| to the
// appropriate ShadowElevation.
ShadowElevation GetShadowElevationConvertDefault(aura::Window* window) {
  ShadowElevation elevation = window->GetProperty(kShadowElevationKey);
  return elevation == ShadowElevation::DEFAULT
             ? GetDefaultShadowElevationForWindow(window)
             : elevation;
}

ShadowElevation GetShadowElevationForActiveState(aura::Window* window) {
  ShadowElevation elevation = window->GetProperty(kShadowElevationKey);
  if (elevation != ShadowElevation::DEFAULT)
    return elevation;

  if (IsActiveWindow(window))
    return ShadowController::kActiveNormalShadowElevation;

  return GetDefaultShadowElevationForWindow(window);
}

// Returns the shadow style to be applied to |losing_active| when it is losing
// active to |gaining_active|. |gaining_active| may be of a type that hides when
// inactive, and as such we do not want to render |losing_active| as inactive.
ShadowElevation GetShadowElevationForWindowLosingActive(
    aura::Window* losing_active,
    aura::Window* gaining_active) {
  if (gaining_active && aura::client::GetHideOnDeactivate(gaining_active)) {
    aura::Window::Windows::const_iterator it =
        std::find(GetTransientChildren(losing_active).begin(),
                  GetTransientChildren(losing_active).end(),
                  gaining_active);
    if (it != GetTransientChildren(losing_active).end())
      return ShadowController::kActiveNormalShadowElevation;
  }
  return kInactiveNormalShadowElevation;
}

}  // namespace

// ShadowController::Impl ------------------------------------------------------

// Real implementation of the ShadowController. ShadowController observes
// ActivationChangeObserver, which are per ActivationClient, where as there is
// only a single Impl (as it observes all window creation by way of an
// EnvObserver).
class ShadowController::Impl :
      public aura::EnvObserver,
      public aura::WindowObserver,
      public base::RefCounted<Impl> {
 public:
  // Returns the singleton instance, destroyed when there are no more refs.
  static Impl* GetInstance();

  // aura::EnvObserver override:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver overrides:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  friend class base::RefCounted<Impl>;
  friend class ShadowController;

  Impl();
  ~Impl() override;

  // Forwarded from ShadowController.
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active);

  // Checks if |window| is visible and contains a property requesting a shadow.
  bool ShouldShowShadowForWindow(aura::Window* window) const;

  // Updates the shadow for windows when activation changes.
  void HandleWindowActivationChange(aura::Window* gaining_active,
                                    aura::Window* losing_active);

  // Shows or hides |window|'s shadow as needed (creating the shadow if
  // necessary).
  void HandlePossibleShadowVisibilityChange(aura::Window* window);

  // Creates a new shadow for |window| and stores it with the |kShadowLayerKey|
  // key.
  // The shadow's bounds are initialized and it is added to the window's layer.
  void CreateShadowForWindow(aura::Window* window);

  ScopedObserver<aura::Window, aura::WindowObserver> observer_manager_;

  static Impl* instance_;

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

// static
ShadowController::Impl* ShadowController::Impl::instance_ = NULL;

// static
ShadowController::Impl* ShadowController::Impl::GetInstance() {
  if (!instance_)
    instance_ = new Impl();
  return instance_;
}

void ShadowController::Impl::OnWindowInitialized(aura::Window* window) {
  observer_manager_.Add(window);
  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::Impl::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  if (key == kShadowElevationKey) {
    if (window->GetProperty(kShadowElevationKey) ==
        static_cast<ShadowElevation>(old))
      return;
  } else if (key == aura::client::kShowStateKey) {
    if (window->GetProperty(aura::client::kShowStateKey) ==
        static_cast<ui::WindowShowState>(old)) {
      return;
    }
  } else {
    return;
  }
  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::Impl::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->SetContentBounds(gfx::Rect(new_bounds.size()));
}

void ShadowController::Impl::OnWindowDestroyed(aura::Window* window) {
  window->ClearProperty(kShadowLayerKey);
  observer_manager_.Remove(window);
}

void ShadowController::Impl::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  if (gained_active) {
    Shadow* shadow = GetShadowForWindow(gained_active);
    if (shadow)
      shadow->SetElevation(GetShadowElevationForActiveState(gained_active));
  }
  if (lost_active) {
    Shadow* shadow = GetShadowForWindow(lost_active);
    if (shadow &&
        GetShadowElevationConvertDefault(lost_active) ==
            kInactiveNormalShadowElevation) {
      shadow->SetElevation(
          GetShadowElevationForWindowLosingActive(lost_active, gained_active));
    }
  }
}

bool ShadowController::Impl::ShouldShowShadowForWindow(
    aura::Window* window) const {
  ui::WindowShowState show_state =
      window->GetProperty(aura::client::kShowStateKey);
  if (show_state == ui::SHOW_STATE_FULLSCREEN ||
      show_state == ui::SHOW_STATE_MAXIMIZED) {
    return false;
  }

  return static_cast<int>(GetShadowElevationConvertDefault(window)) > 0;
}

void ShadowController::Impl::HandlePossibleShadowVisibilityChange(
    aura::Window* window) {
  const bool should_show = ShouldShowShadowForWindow(window);
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow) {
    shadow->SetElevation(GetShadowElevationForActiveState(window));
    shadow->layer()->SetVisible(should_show);
  } else if (should_show && !shadow) {
    CreateShadowForWindow(window);
  }
}

void ShadowController::Impl::CreateShadowForWindow(aura::Window* window) {
  Shadow* shadow = new Shadow();
  window->SetProperty(kShadowLayerKey, shadow);
  shadow->Init(GetShadowElevationForActiveState(window));
  shadow->SetContentBounds(gfx::Rect(window->bounds().size()));
  shadow->layer()->SetVisible(ShouldShowShadowForWindow(window));
  window->layer()->Add(shadow->layer());
}

ShadowController::Impl::Impl()
    : observer_manager_(this) {
  aura::Env::GetInstance()->AddObserver(this);
}

ShadowController::Impl::~Impl() {
  DCHECK_EQ(instance_, this);
  aura::Env::GetInstance()->RemoveObserver(this);
  instance_ = NULL;
}

// ShadowController ------------------------------------------------------------

Shadow* ShadowController::GetShadowForWindow(aura::Window* window) {
  return window->GetProperty(kShadowLayerKey);
}

ShadowController::ShadowController(
    aura::client::ActivationClient* activation_client)
    : activation_client_(activation_client),
      impl_(Impl::GetInstance()) {
  // Watch for window activation changes.
  activation_client_->AddObserver(this);
}

ShadowController::~ShadowController() {
  activation_client_->RemoveObserver(this);
}

void ShadowController::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  impl_->OnWindowActivated(reason, gained_active, lost_active);
}

}  // namespace wm
