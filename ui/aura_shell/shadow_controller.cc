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
}

void ShadowController::OnWindowParentChanged(aura::Window* window,
                                             aura::Window* parent) {
  Shadow* shadow = GetShadowForWindow(window);

  if (parent) {
    if (shadow) {
      parent->layer()->Add(shadow->layer());
      StackShadowBelowWindow(shadow, window);
    } else {
      if (ShouldShowShadowForWindow(window))
        CreateShadowForWindow(window);
    }
  } else {
    if (shadow && shadow->layer()->parent())
      shadow->layer()->parent()->Remove(shadow->layer());
  }
}

void ShadowController::OnWindowPropertyChanged(aura::Window* window,
                                               const char* name,
                                               void* old) {
  if (name == aura::kShadowTypeKey)
    HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::OnWindowVisibilityChanged(aura::Window* window,
                                                 bool visible) {
  HandlePossibleShadowVisibilityChange(window);
}

void ShadowController::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& bounds) {
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    shadow->SetContentBounds(bounds);
}

void ShadowController::OnWindowStackingChanged(aura::Window* window) {
  Shadow* shadow = GetShadowForWindow(window);
  if (shadow)
    StackShadowBelowWindow(shadow, window);
}

void ShadowController::OnWindowDestroyed(aura::Window* window) {
  window_shadows_.erase(window);
}

bool ShadowController::ShouldShowShadowForWindow(aura::Window* window) const {
  const aura::ShadowType type = static_cast<aura::ShadowType>(
      window->GetIntProperty(aura::kShadowTypeKey));
  bool requested = false;
  switch (type) {
    case aura::SHADOW_TYPE_NONE:
      break;
    case aura::SHADOW_TYPE_RECTANGULAR:
      requested = true;
      break;
    default:
      NOTREACHED() << "Unknown shadow type " << type;
  }

  return requested && window->layer()->visible();
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
  shadow->SetContentBounds(window->bounds());
  shadow->layer()->SetVisible(ShouldShowShadowForWindow(window));

  if (window->parent()) {
    window->parent()->layer()->Add(shadow->layer());
    StackShadowBelowWindow(shadow.get(), window);
  }
}

void ShadowController::StackShadowBelowWindow(Shadow* shadow,
                                              aura::Window* window) {
  ui::Layer* parent_layer = window->parent()->layer();
  DCHECK_EQ(shadow->layer()->parent(), parent_layer);

  // TODO(derat): Add a StackBelow() method and use that instead (although we
  // then run the risk of other layers getting stacked between a window and its
  // shadow).
  parent_layer->StackAbove(shadow->layer(), window->layer());
  parent_layer->StackAbove(window->layer(), shadow->layer());
}

}  // namespace internal
}  // namespace aura_shell
