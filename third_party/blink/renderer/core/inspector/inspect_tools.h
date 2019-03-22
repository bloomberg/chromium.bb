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

// -----------------------------------------------------------------------------

class SearchingForNodeTool : public InspectTool {
 public:
  SearchingForNodeTool(InspectorDOMAgent* dom_agent,
                       bool ua_shadow,
                       const String& highlight_config);

 private:
  bool HandleInputEvent(LocalFrameView* frame_view,
                        const WebInputEvent& input_event,
                        bool* swallow_next_mouse_up,
                        bool* swallow_next_escape_up) override;
  CString GetDataResourceName() override;
  bool HandleMouseDown(const WebMouseEvent& event,
                       bool* swallow_next_mouse_up) override;
  bool HandleMouseMove(const WebMouseEvent& event) override;
  bool HandleGestureTapEvent(const WebGestureEvent&) override;
  bool HandlePointerEvent(const WebPointerEvent&) override;
  void Draw(float scale) override;
  void NodeHighlightRequested(Node*);
  void Trace(blink::Visitor* visitor) override;

  Member<InspectorDOMAgent> dom_agent_;
  bool ua_shadow_;
  Member<Node> hovered_node_;
  Member<Node> event_target_node_;
  std::unique_ptr<InspectorHighlightConfig> highlight_config_;
  bool omit_tooltip_ = false;
  DISALLOW_COPY_AND_ASSIGN(SearchingForNodeTool);
};

// -----------------------------------------------------------------------------

class QuadHighlightTool : public InspectTool {
 public:
  QuadHighlightTool(std::unique_ptr<FloatQuad> quad,
                    Color color,
                    Color outline_color);

 private:
  bool ForwardEventsToOverlay() override;
  void Draw(float scale) override;
  std::unique_ptr<FloatQuad> quad_;
  Color color_;
  Color outline_color_;
  DISALLOW_COPY_AND_ASSIGN(QuadHighlightTool);
};

// -----------------------------------------------------------------------------

class NodeHighlightTool : public InspectTool {
 public:
  NodeHighlightTool(Member<Node> node,
                    String selector_list,
                    std::unique_ptr<InspectorHighlightConfig> highlight_config);

 private:
  CString GetDataResourceName() override;
  bool ForwardEventsToOverlay() override;
  void Draw(float scale) override;
  void DrawNode();
  void DrawMatchingSelector();
  void Trace(blink::Visitor* visitor) override;

  Member<Node> node_;
  String selector_list_;
  std::unique_ptr<InspectorHighlightConfig> highlight_config_;
  DISALLOW_COPY_AND_ASSIGN(NodeHighlightTool);
};

// -----------------------------------------------------------------------------

class ShowViewSizeTool : public InspectTool {
 public:
  ShowViewSizeTool() = default;

 private:
  bool ForwardEventsToOverlay() override;
  CString GetDataResourceName() override;
  void Draw(float scale) override;
  DISALLOW_COPY_AND_ASSIGN(ShowViewSizeTool);
};

// -----------------------------------------------------------------------------

class ScreenshotTool : public InspectTool {
 public:
  ScreenshotTool() = default;

 private:
  CString GetDataResourceName() override;
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
  DISALLOW_COPY_AND_ASSIGN(ScreenshotTool);
};

// -----------------------------------------------------------------------------

class PausedInDebuggerTool : public InspectTool {
 public:
  explicit PausedInDebuggerTool(const String& message) : message_(message) {}

 private:
  CString GetDataResourceName() override;
  void Draw(float scale) override;
  String message_;
  DISALLOW_COPY_AND_ASSIGN(PausedInDebuggerTool);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECT_TOOLS_H_
