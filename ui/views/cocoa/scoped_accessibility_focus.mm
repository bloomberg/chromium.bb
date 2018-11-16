// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/scoped_accessibility_focus.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#include "ui/views_bridge_mac/bridged_native_widget_host_helper.h"

namespace views {

namespace {
bool g_has_swizzled_method = false;
views_bridge_mac::BridgedNativeWidgetHostHelper* g_overridden_key_element = nil;

// Swizzled method for -[NSApplication accessibilityFocusedUIElement]. This
// will return the overridden element if one is present. Otherwise, it will
// return the key window.
id SwizzledAccessibilityFocusedUIElement(NSApplication* app, SEL) {
  if (g_overridden_key_element) {
    return [g_overridden_key_element->GetNativeViewAccessible()
                accessibilityFocusedUIElement];
  }
  return [app keyWindow];
}
}  // namespace

ScopedAccessibilityFocus::ScopedAccessibilityFocus(
    views_bridge_mac::BridgedNativeWidgetHostHelper* host_helper)
    : host_helper_(host_helper) {
  if (!g_has_swizzled_method) {
    Method method = class_getInstanceMethod(
        [NSApplication class], @selector(accessibilityFocusedUIElement));
    method_setImplementation(method,
                             (IMP)SwizzledAccessibilityFocusedUIElement);
    g_has_swizzled_method = true;
  }
  g_overridden_key_element = host_helper_;
}

ScopedAccessibilityFocus::~ScopedAccessibilityFocus() {
  if (g_overridden_key_element == host_helper_)
    g_overridden_key_element = nullptr;
}

}  // namespace views
