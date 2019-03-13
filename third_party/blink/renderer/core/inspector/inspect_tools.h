// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECT_TOOLS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECT_TOOLS_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"

namespace blink {

class WebMouseEvent;
class WebPointerEvent;

class SearchingForNodeTool : public InspectTool {
 public:
  SearchingForNodeTool(InspectorDOMAgent* dom_agent,
                       bool ua_shadow,
                       const String& highlight_config);

 private:
  bool HandleMouseDown(const WebMouseEvent& event,
                       bool* swallow_next_mouse_up) override;
  bool HandleMouseMove(const WebMouseEvent& event) override;
  bool HandleGestureTapEvent(const WebGestureEvent&) override;
  bool HandlePointerEvent(const WebPointerEvent&) override;
  void NodeHighlightRequested(Node*);
  void Trace(blink::Visitor* visitor) override;

  Member<InspectorDOMAgent> dom_agent_;
  bool ua_shadow_;
  Member<Node> hovered_node_for_inspect_mode_;
  std::unique_ptr<InspectorHighlightConfig> inspect_mode_highlight_config_;
};

class ScreenshotTool : public InspectTool {
 public:
  ScreenshotTool() = default;

 private:
  bool HandleKeyboardEvent(const WebKeyboardEvent&,
                           bool* swallow_next_escape_up) override;
  bool HandleMouseDown(const WebMouseEvent& event,
                       bool* swallow_next_mouse_up) override;
  bool HandleMouseMove(const WebMouseEvent& event) override;
  bool HandleMouseUp(const WebMouseEvent& event) override;
  void Draw(float scale) override;
  void DoInit() override;

  IntPoint screenshot_anchor_;
  IntPoint screenshot_position_;
};

class PausedInDebuggerTool : public InspectTool {
 public:
  explicit PausedInDebuggerTool(const String& message) : message_(message) {}

 private:
  void Draw(float scale) override;
  String message_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECT_TOOLS_H_
