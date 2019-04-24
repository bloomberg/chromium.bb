// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node.h"

#include "base/lazy_instance.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/buildflags.h"

namespace ui {

// static
base::LazyInstance<base::ObserverList<AXModeObserver>::Unchecked>::Leaky
    AXPlatformNode::ax_mode_observers_ = LAZY_INSTANCE_INITIALIZER;

// static
base::LazyInstance<AXPlatformNode::NativeWindowHandlerCallback>::Leaky
    AXPlatformNode::native_window_handler_ = LAZY_INSTANCE_INITIALIZER;

// static
AXMode AXPlatformNode::ax_mode_;

// static
bool AXPlatformNode::has_input_suggestions_ = false;

// static
gfx::NativeViewAccessible AXPlatformNode::popup_focus_override_ = nullptr;

// static
AXPlatformNode* AXPlatformNode::FromNativeWindow(
    gfx::NativeWindow native_window) {
  if (native_window_handler_.Get())
    return native_window_handler_.Get().Run(native_window);
  return nullptr;
}

#if !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()
// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return nullptr;
}
#endif  // !BUILDFLAG_INTERNAL_HAS_NATIVE_ACCESSIBILITY()

// static
void AXPlatformNode::RegisterNativeWindowHandler(
    AXPlatformNode::NativeWindowHandlerCallback handler) {
  native_window_handler_.Get() = handler;
}

AXPlatformNode::AXPlatformNode() {}

AXPlatformNode::~AXPlatformNode() {
}

void AXPlatformNode::Destroy() {
}

int32_t AXPlatformNode::GetUniqueId() const {
  DCHECK(GetDelegate()) << "|GetUniqueId| must be called after |Init|.";
  return GetDelegate() ? GetDelegate()->GetUniqueId().Get() : -1;
}

// static
void AXPlatformNode::AddAXModeObserver(AXModeObserver* observer) {
  ax_mode_observers_.Get().AddObserver(observer);
}

// static
void AXPlatformNode::RemoveAXModeObserver(AXModeObserver* observer) {
  ax_mode_observers_.Get().RemoveObserver(observer);
}

// static
void AXPlatformNode::NotifyAddAXModeFlags(AXMode mode_flags) {
  ax_mode_ |= mode_flags;
  for (auto& observer : ax_mode_observers_.Get())
    observer.OnAXModeAdded(mode_flags);
}

// static
void AXPlatformNode::OnInputSuggestionsAvailable() {
  has_input_suggestions_ = true;
}

// static
void AXPlatformNode::OnInputSuggestionsUnavailable() {
  has_input_suggestions_ = false;
}

// static
// TODO(crbug.com/865101) Remove this once the autofill state works.
bool AXPlatformNode::HasInputSuggestions() {
  return has_input_suggestions_;
}

// static
void AXPlatformNode::SetPopupFocusOverride(
    gfx::NativeViewAccessible popup_focus_override) {
  popup_focus_override_ = popup_focus_override;
}

// static
gfx::NativeViewAccessible AXPlatformNode::GetPopupFocusOverride() {
  return popup_focus_override_;
}

}  // namespace ui
