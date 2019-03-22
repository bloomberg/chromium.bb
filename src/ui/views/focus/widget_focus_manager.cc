// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/widget_focus_manager.h"

#include "base/supports_user_data.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#endif

namespace views {

#if defined(USE_AURA)
namespace {

const char kWidgetFocusManagerKey[] = "WidgetFocusManager";

}  // namespace

class WidgetFocusManager::Owner : public base::SupportsUserData::Data {
 public:
  explicit Owner(std::unique_ptr<WidgetFocusManager> focus_manager)
      : focus_manager_(std::move(focus_manager)) {}
  ~Owner() override = default;

  WidgetFocusManager* focus_manager() { return focus_manager_.get(); }

 private:
  std::unique_ptr<WidgetFocusManager> focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(Owner);
};

#endif

// WidgetFocusManager ----------------------------------------------------------

// static
WidgetFocusManager* WidgetFocusManager::GetInstance(gfx::NativeWindow context) {
#if defined(USE_AURA)
  // With aura there may be multiple Envs, in such a situation the
  // WidgetFocusManager needs to be per Env.
  aura::Env* env = context ? context->env() : aura::Env::GetInstance();
  DCHECK(env);
  Owner* owner = static_cast<Owner*>(env->GetUserData(kWidgetFocusManagerKey));
  if (!owner) {
    std::unique_ptr<Owner> owner_ptr =
        std::make_unique<Owner>(base::WrapUnique(new WidgetFocusManager()));
    owner = owner_ptr.get();
    env->SetUserData(kWidgetFocusManagerKey, std::move(owner_ptr));
  }
  return owner->focus_manager();
#else
  static base::NoDestructor<WidgetFocusManager> instance;
  return instance.get();
#endif
}

WidgetFocusManager::~WidgetFocusManager() = default;

void WidgetFocusManager::AddFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  focus_change_listeners_.AddObserver(listener);
}

void WidgetFocusManager::RemoveFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  focus_change_listeners_.RemoveObserver(listener);
}

void WidgetFocusManager::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (enabled_) {
    for (WidgetFocusChangeListener& observer : focus_change_listeners_)
      observer.OnNativeFocusChanged(focused_now);
  }
}

WidgetFocusManager::WidgetFocusManager() : enabled_(true) {}

// AutoNativeNotificationDisabler ----------------------------------------------

AutoNativeNotificationDisabler::AutoNativeNotificationDisabler() {
  WidgetFocusManager::GetInstance()->DisableNotifications();
}

AutoNativeNotificationDisabler::~AutoNativeNotificationDisabler() {
  WidgetFocusManager::GetInstance()->EnableNotifications();
}

}  // namespace views
