// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/accessibility_focus_overrider.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

namespace ui {

namespace {
bool g_has_swizzled_method = false;
AccessibilityFocusOverrider::Client* g_overridden_focused_element = nil;

// Swizzled method for -[NSApplication accessibilityFocusedUIElement]. This
// will return the overridden element if one is present. Otherwise, it will
// return the key window.
id SwizzledAccessibilityFocusedUIElement(NSApplication* app, SEL) {
  if (g_overridden_focused_element)
    return g_overridden_focused_element->GetAccessibilityFocusedUIElement();
  return [[app keyWindow] accessibilityFocusedUIElement];
}
}  // namespace

AccessibilityFocusOverrider::AccessibilityFocusOverrider(Client* client)
    : client_(client) {
  if (!g_has_swizzled_method) {
    Method method = class_getInstanceMethod(
        [NSApplication class], @selector(accessibilityFocusedUIElement));
    method_setImplementation(method,
                             (IMP)SwizzledAccessibilityFocusedUIElement);
    g_has_swizzled_method = true;
  }
}

AccessibilityFocusOverrider::~AccessibilityFocusOverrider() {
  if (g_overridden_focused_element == client_)
    g_overridden_focused_element = nullptr;
}

void AccessibilityFocusOverrider::SetWindowIsKey(bool window_is_key) {
  window_is_key_ = window_is_key;
  UpdateOverriddenKeyElement();
}

void AccessibilityFocusOverrider::SetViewIsFirstResponder(
    bool view_is_first_responder) {
  view_is_first_responder_ = view_is_first_responder;
  UpdateOverriddenKeyElement();
}

void AccessibilityFocusOverrider::UpdateOverriddenKeyElement() {
  if (window_is_key_ && view_is_first_responder_) {
    g_overridden_focused_element = client_;
  } else if (g_overridden_focused_element == client_) {
    g_overridden_focused_element = nullptr;
  }
}

}  // namespace ui
