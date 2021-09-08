// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_HIGHLIGHT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_HIGHLIGHT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/inspector/node_content_visibility_state.h"
#include "third_party/blink/renderer/core/inspector/protocol/DOM.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Color;

enum class ColorFormat { RGB, HEX, HSL };
enum class ContrastAlgorithm { AA, AAA, APCA };

struct CORE_EXPORT LineStyle {
  USING_FAST_MALLOC(LineStyle);

 public:
  LineStyle();

  bool IsTransparent() const { return color == Color::kTransparent; }

  Color color;
  String pattern;
};

struct CORE_EXPORT BoxStyle {
  USING_FAST_MALLOC(BoxStyle);

 public:
  BoxStyle();

  bool IsTransparent() const {
    return fill_color == Color::kTransparent &&
           hatch_color == Color::kTransparent;
  }

  Color fill_color;
  Color hatch_color;
};

struct CORE_EXPORT InspectorSourceOrderConfig {
  USING_FAST_MALLOC(InspectorSourceOrderConfig);

 public:
  InspectorSourceOrderConfig();

  Color parent_outline_color;
  Color child_outline_color;
};

struct CORE_EXPORT InspectorGridHighlightConfig {
  USING_FAST_MALLOC(InspectorGridHighlightConfig);

 public:
  InspectorGridHighlightConfig();

  Color grid_color;
  Color row_line_color;
  Color column_line_color;
  Color row_gap_color;
  Color column_gap_color;
  Color row_hatch_color;
  Color column_hatch_color;
  Color area_border_color;
  Color grid_background_color;

  bool show_grid_extension_lines;
  bool grid_border_dash;
  bool row_line_dash;
  bool column_line_dash;
  bool show_positive_line_numbers;
  bool show_negative_line_numbers;
  bool show_area_names;
  bool show_line_names;
  bool show_track_sizes;
};

struct CORE_EXPORT InspectorFlexContainerHighlightConfig {
  USING_FAST_MALLOC(InspectorFlexContainerHighlightConfig);

 public:
  InspectorFlexContainerHighlightConfig();

  absl::optional<LineStyle> container_border;
  absl::optional<LineStyle> line_separator;
  absl::optional<LineStyle> item_separator;

  absl::optional<BoxStyle> main_distributed_space;
  absl::optional<BoxStyle> cross_distributed_space;
  absl::optional<BoxStyle> row_gap_space;
  absl::optional<BoxStyle> column_gap_space;
  absl::optional<LineStyle> cross_alignment;
};

struct CORE_EXPORT InspectorScrollSnapContainerHighlightConfig {
  USING_FAST_MALLOC(InspectorScrollSnapContainerHighlightConfig);

 public:
  InspectorScrollSnapContainerHighlightConfig() = default;

  absl::optional<LineStyle> snapport_border;
  absl::optional<LineStyle> snap_area_border;

  Color scroll_margin_color;
  Color scroll_padding_color;
};

struct CORE_EXPORT InspectorFlexItemHighlightConfig {
  USING_FAST_MALLOC(InspectorFlexItemHighlightConfig);

 public:
  InspectorFlexItemHighlightConfig();

  absl::optional<BoxStyle> base_size_box;
  absl::optional<LineStyle> base_size_border;
  absl::optional<LineStyle> flexibility_arrow;
};

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
  bool show_styles;
  bool show_rulers;
  bool show_extension_lines;
  bool show_accessibility_info;

  String selector_list;
  ColorFormat color_format = ColorFormat::HEX;
  ContrastAlgorithm contrast_algorithm = ContrastAlgorithm::AA;

  std::unique_ptr<InspectorGridHighlightConfig> grid_highlight_config;
  std::unique_ptr<InspectorFlexContainerHighlightConfig>
      flex_container_highlight_config;
  std::unique_ptr<InspectorFlexItemHighlightConfig> flex_item_highlight_config;
};

struct InspectorHighlightContrastInfo {
  Color background_color;
  String font_size;
  String font_weight;
  float text_opacity;
};

class InspectorHighlightBase {
 public:
  explicit InspectorHighlightBase(float scale);
  explicit InspectorHighlightBase(Node*);
  void AppendPath(std::unique_ptr<protocol::ListValue> path,
                  const Color& fill_color,
                  const Color& outline_color,
                  const String& name = String());
  void AppendQuad(const FloatQuad&,
                  const Color& fill_color,
                  const Color& outline_color = Color::kTransparent,
                  const String& name = String());
  virtual std::unique_ptr<protocol::DictionaryValue> AsProtocolValue()
      const = 0;

 protected:
  static bool BuildNodeQuads(Node*,
                             FloatQuad* content,
                             FloatQuad* padding,
                             FloatQuad* border,
                             FloatQuad* margin);
  std::unique_ptr<protocol::ListValue> highlight_paths_;
  float scale_;
};

class CORE_EXPORT InspectorSourceOrderHighlight
    : public InspectorHighlightBase {
  STACK_ALLOCATED();

 public:
  InspectorSourceOrderHighlight(Node*, Color, int source_order_position);
  static InspectorSourceOrderConfig DefaultConfig();
  std::unique_ptr<protocol::DictionaryValue> AsProtocolValue() const override;

 private:
  int source_order_position_;
};

class CORE_EXPORT InspectorHighlight : public InspectorHighlightBase {
  STACK_ALLOCATED();

 public:
  InspectorHighlight(Node*,
                     const InspectorHighlightConfig&,
                     const InspectorHighlightContrastInfo&,
                     bool append_element_info,
                     bool append_distance_info,
                     NodeContentVisibilityState content_visibility_state);
  explicit InspectorHighlight(float scale);
  ~InspectorHighlight();

  static bool GetBoxModel(Node*,
                          std::unique_ptr<protocol::DOM::BoxModel>*,
                          bool use_absolute_zoom);
  static bool GetContentQuads(
      Node*,
      std::unique_ptr<protocol::Array<protocol::Array<double>>>*);
  static InspectorHighlightConfig DefaultConfig();
  static InspectorGridHighlightConfig DefaultGridConfig();
  static InspectorFlexContainerHighlightConfig DefaultFlexContainerConfig();
  static InspectorFlexItemHighlightConfig DefaultFlexItemConfig();
  void AppendEventTargetQuads(Node* event_target_node,
                              const InspectorHighlightConfig&);
  std::unique_ptr<protocol::DictionaryValue> AsProtocolValue() const override;

 private:
  static bool BuildSVGQuads(Node*, Vector<FloatQuad>& quads);
  void AppendNodeHighlight(Node*, const InspectorHighlightConfig&);
  void AppendPathsForShapeOutside(Node*, const InspectorHighlightConfig&);

  void AppendDistanceInfo(Node* node);
  void VisitAndCollectDistanceInfo(Node* node);
  void VisitAndCollectDistanceInfo(PseudoId pseudo_id,
                                   LayoutObject* layout_object);
  void AddLayoutBoxToDistanceInfo(LayoutObject* layout_object);

  static LineStyle DefaultLineStyle();
  static BoxStyle DefaultBoxStyle();

  std::unique_ptr<protocol::Array<protocol::Array<double>>> boxes_;
  std::unique_ptr<protocol::DictionaryValue> computed_style_;
  std::unique_ptr<protocol::DOM::BoxModel> model_;
  std::unique_ptr<protocol::DictionaryValue> distance_info_;
  std::unique_ptr<protocol::DictionaryValue> element_info_;
  std::unique_ptr<protocol::ListValue> grid_info_;
  std::unique_ptr<protocol::ListValue> flex_container_info_;
  std::unique_ptr<protocol::ListValue> flex_item_info_;
  bool show_rulers_;
  bool show_extension_lines_;
  bool show_accessibility_info_;
  ColorFormat color_format_;
};

std::unique_ptr<protocol::DictionaryValue> InspectorGridHighlight(
    Node*,
    const InspectorGridHighlightConfig& config);

std::unique_ptr<protocol::DictionaryValue> InspectorFlexContainerHighlight(
    Node* node,
    const InspectorFlexContainerHighlightConfig& config);

std::unique_ptr<protocol::DictionaryValue> InspectorScrollSnapHighlight(
    Node* node,
    const InspectorScrollSnapContainerHighlightConfig& config);

// CORE_EXPORT is required to make the function available for unit tests.
std::unique_ptr<protocol::DictionaryValue> CORE_EXPORT
BuildSnapContainerInfo(Node* node);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_HIGHLIGHT_H_
