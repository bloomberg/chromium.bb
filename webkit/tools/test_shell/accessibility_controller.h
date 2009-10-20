// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_CONTROLLER_H_
#define WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_CONTROLLER_H_

#include "webkit/glue/cpp_bound_class.h"
#include "webkit/tools/test_shell/accessibility_ui_element.h"

namespace WebKit {
class WebAccessibilityObject;
class WebFrame;
}

class AccessibilityUIElement;
class AccessibilityUIElementList;
class TestShell;

class AccessibilityController : public CppBoundClass {
 public:
  explicit AccessibilityController(TestShell* shell);

  // shadow to include accessibility initialization.
  void BindToJavascript(
      WebKit::WebFrame* frame, const std::wstring& classname);
  void Reset();

  void SetFocusedElement(const WebKit::WebAccessibilityObject& focused_element);
  AccessibilityUIElement* GetFocusedElement();
  AccessibilityUIElement* GetRootElement();

 private:
  // Bound methods and properties
  void LogFocusEventsCallback(const CppArgumentList& args, CppVariant* result);
  void LogScrollingStartEventsCallback(
      const CppArgumentList& args, CppVariant* result);
  void FallbackCallback(const CppArgumentList& args, CppVariant* result);

  void FocusedElementGetterCallback(CppVariant* result);
  void RootElementGetterCallback(CppVariant* result);

  WebKit::WebAccessibilityObject focused_element_;
  WebKit::WebAccessibilityObject root_element_;

  AccessibilityUIElementList elements_;

  TestShell* shell_;
};

#endif  // WEBKIT_TOOLS_TEST_SHELL_ACCESSIBILITY_CONTROLLER_H_
