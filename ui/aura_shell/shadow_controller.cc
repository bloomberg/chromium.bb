// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shadow_controller.h"

#include <utility>

#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/shadow_types.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/shadow.h"

using std::make_pair;

namespace aura_shell {
namespace internal {

ShadowController::ShadowController() {
  aura::Desktop::GetInstance()->AddObserver(this);
}

ShadowController::~ShadowController() {
  for (WindowShadowMap::const_iterator it = window_shadows_.begin();
       it != window_shadows_.end(); ++it) {
    it->first->RemoveObserver(this);
  }
  aura::Desktop::GetInstance()->RemoveObserver(this);
}

void ShadowController::OnWindowInitialized(aura::Window* window) {
  window->AddObserver(this);
  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::OnWindowPropertyChanged(aura::Window* window,
                                               const char* name,
                                               void* old) {
  if (name == aura::kShadowTypeKey)
    HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& bounds) {
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->SetContentBounds(gfx::Rect(bounds.size()));
}

void ShadowController::OnWindowDestroyed(aura::Window* window) {
  window_shadows_.erase(window);
}

bool ShadowController::ShouldShowShadowForWindow(aura::Window* window) const {
  const aura::ShadowType type = static_cast<aura::ShadowType>(
      window->GetIntProperty(aura::kShadowTypeKey));
  switch (type) {
    case aura::SHADOW_TYPE_NONE:
      return false;
    case aura::SHADOW_TYPE_RECTANGULAR:
      return true;
    default:
      NOTREACHED() << "Unknown shadow type " << type;
      return false;
  }
}

Shadow* ShadowController::GetShadowForWindow(aura::Window* window) {
  WindowShadowMap::const_iterator it = window_shadows_.find(window);
  return it != window_shadows_.end() ? it->second.get() : NULL;
}

void ShadowController::HandlePossibleShadowVisibilityChange(
    aura::Window* window) {
  const bool should_show = ShouldShowShadowForWindow(window);
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->layer()->SetVisible(should_show);
  else if (should_show && !shadow)
    CreateShadowForWindow(window);
}

void ShadowController::CreateShadowForWindow(aura::Window* window) {
  linked_ptr<Shadow> shadow(new Shadow());
  window_shadows_.insert(make_pair(window, shadow));

  shadow->Init();
  shadow->SetContentBounds(gfx::Rect(window->bounds().size()));
  shadow->layer()->SetVisible(ShouldShowShadowForWindow(window));
  window->layer()->Add(shadow->layer());
}

}  // namespace internal
}  // namespace aura_shell
