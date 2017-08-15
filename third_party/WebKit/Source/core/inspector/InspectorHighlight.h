// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorHighlight_h
#define InspectorHighlight_h

#include "core/CoreExport.h"
#include "core/inspector/protocol/DOM.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"

namespace blink {

class Color;

struct CORE_EXPORT InspectorHighlightConfig {
  USING_FAST_MALLOC(InspectorHighlightConfig);

 public:
  InspectorHighlightConfig();

  Color content;
  Color content_outline;
  Color padding;
  Color border;
  Color margin;
  Color event_target;
  Color shape;
  Color shape_margin;
  Color css_grid;

  bool show_info;
  bool show_rulers;
  bool show_extension_lines;
  bool display_as_material;

  String selector_list;
};

class CORE_EXPORT InspectorHighlight {
  STACK_ALLOCATED();

 public:
  InspectorHighlight(Node*,
                     const InspectorHighlightConfig&,
                     bool append_element_info);
  explicit InspectorHighlight(float scale);
  ~InspectorHighlight();

  static bool GetBoxModel(Node*, std::unique_ptr<protocol::DOM::BoxModel>*);
  static InspectorHighlightConfig DefaultConfig();
  static bool BuildNodeQuads(Node*,
                             FloatQuad* content,
                             FloatQuad* padding,
                             FloatQuad* border,
                             FloatQuad* margin);

  void AppendPath(std::unique_ptr<protocol::ListValue> path,
                  const Color& fill_color,
                  const Color& outline_color,
                  const String& name = String());
  void AppendQuad(const FloatQuad&,
                  const Color& fill_color,
                  const Color& outline_color = Color::kTransparent,
                  const String& name = String());
  void AppendEventTargetQuads(Node* event_target_node,
                              const InspectorHighlightConfig&);
  std::unique_ptr<protocol::DictionaryValue> AsProtocolValue() const;

 private:
  void AppendNodeHighlight(Node*, const InspectorHighlightConfig&);
  void AppendPathsForShapeOutside(Node*, const InspectorHighlightConfig&);

  std::unique_ptr<protocol::DictionaryValue> element_info_;
  std::unique_ptr<protocol::ListValue> highlight_paths_;
  std::unique_ptr<protocol::DictionaryValue> grid_info_;
  bool show_rulers_;
  bool show_extension_lines_;
  bool display_as_material_;
  float scale_;
};

}  // namespace blink

#endif  // InspectorHighlight_h
