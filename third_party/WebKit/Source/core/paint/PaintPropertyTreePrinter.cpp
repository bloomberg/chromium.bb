// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintPropertyTreePrinter.h"

#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutView.h"
#include "core/paint/ObjectPaintProperties.h"
#include "platform/graphics/paint/PropertyTreeState.h"

#include <iomanip>
#include <sstream>

#if DCHECK_IS_ON()

namespace blink {
namespace {

template <typename PropertyTreeNode>
class PropertyTreePrinterTraits;

template <typename PropertyTreeNode>
class PropertyTreePrinter {
 public:
  String TreeAsString(const LocalFrameView& frame_view) {
    CollectPropertyNodes(frame_view);

    const PropertyTreeNode* root_node = LookupRootNode();
    if (!root_node)
      return "";

    if (!node_to_debug_string_.Contains(root_node))
      AddPropertyNode(root_node, "root");

    StringBuilder string_builder;
    AddAllPropertyNodes(string_builder, root_node);
    return string_builder.ToString();
  }

  String PathAsString(const PropertyTreeNode* last_node) {
    const PropertyTreeNode* node = last_node;
    while (!node->IsRoot()) {
      AddPropertyNode(node, "");
      node = node->Parent();
    }
    AddPropertyNode(node, "root");

    StringBuilder string_builder;
    AddAllPropertyNodes(string_builder, node);
    return string_builder.ToString();
  }

  void AddPropertyNode(const PropertyTreeNode* node, String debug_info) {
    node_to_debug_string_.Set(node, debug_info);
  }

 private:
  using Traits = PropertyTreePrinterTraits<PropertyTreeNode>;

  void CollectPropertyNodes(const LocalFrameView& frame_view) {
    Traits::AddFrameViewProperties(frame_view, *this);
    if (LayoutView* layout_view = frame_view.GetLayoutView())
      CollectPropertyNodes(*layout_view);
    for (Frame* child = frame_view.GetFrame().Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (!child->IsLocalFrame())
        continue;
      if (LocalFrameView* child_view = ToLocalFrame(child)->View())
        CollectPropertyNodes(*child_view);
    }
  }

  void CollectPropertyNodes(const LayoutObject& object) {
    if (const ObjectPaintProperties* paint_properties =
            object.FirstFragment() ? object.FirstFragment()->PaintProperties()
                                   : nullptr)
      Traits::AddObjectPaintProperties(object, *paint_properties, *this);
    for (LayoutObject* child = object.SlowFirstChild(); child;
         child = child->NextSibling())
      CollectPropertyNodes(*child);
  }

  void AddAllPropertyNodes(StringBuilder& string_builder,
                           const PropertyTreeNode* node,
                           unsigned indent = 0) {
    DCHECK(node);
    for (unsigned i = 0; i < indent; i++)
      string_builder.Append(' ');
    if (node_to_debug_string_.Contains(node))
      string_builder.Append(node_to_debug_string_.at(node));
    string_builder.Append(String::Format(" %p ", node));
    string_builder.Append(node->ToString());
    string_builder.Append("\n");

    for (const auto* child_node : node_to_debug_string_.Keys()) {
      if (child_node->Parent() == node)
        AddAllPropertyNodes(string_builder, child_node, indent + 2);
    }
  }

  // Root nodes may not be directly accessible but they can be determined by
  // walking up to the parent of a previously collected node.
  const PropertyTreeNode* LookupRootNode() {
    for (const auto* node : node_to_debug_string_.Keys()) {
      if (node->IsRoot())
        return node;
      if (node->Parent() && node->Parent()->IsRoot())
        return node->Parent();
    }
    return nullptr;
  }

  HashMap<const PropertyTreeNode*, String> node_to_debug_string_;
};

template <>
class PropertyTreePrinterTraits<TransformPaintPropertyNode> {
 public:
  static void AddFrameViewProperties(
      const LocalFrameView& frame_view,
      PropertyTreePrinter<TransformPaintPropertyNode>& printer) {
    if (const TransformPaintPropertyNode* pre_translation =
            frame_view.PreTranslation())
      printer.AddPropertyNode(pre_translation, "PreTranslation (FrameView)");
    if (const TransformPaintPropertyNode* scroll_translation =
            frame_view.ScrollTranslation())
      printer.AddPropertyNode(scroll_translation,
                              "ScrollTranslation (FrameView)");
  }

  static void AddObjectPaintProperties(
      const LayoutObject& object,
      const ObjectPaintProperties& paint_properties,
      PropertyTreePrinter<TransformPaintPropertyNode>& printer) {
    if (const TransformPaintPropertyNode* paint_offset_translation =
            paint_properties.PaintOffsetTranslation())
      printer.AddPropertyNode(
          paint_offset_translation,
          "PaintOffsetTranslation (" + object.DebugName() + ")");
    if (const TransformPaintPropertyNode* transform =
            paint_properties.Transform())
      printer.AddPropertyNode(transform,
                              "Transform (" + object.DebugName() + ")");
    if (const TransformPaintPropertyNode* perspective =
            paint_properties.Perspective())
      printer.AddPropertyNode(perspective,
                              "Perspective (" + object.DebugName() + ")");
    if (const TransformPaintPropertyNode* svg_local_to_border_box_transform =
            paint_properties.SvgLocalToBorderBoxTransform())
      printer.AddPropertyNode(
          svg_local_to_border_box_transform,
          "SvgLocalToBorderBoxTransform (" + object.DebugName() + ")");
    if (const TransformPaintPropertyNode* scroll_translation =
            paint_properties.ScrollTranslation())
      printer.AddPropertyNode(scroll_translation,
                              "ScrollTranslation (" + object.DebugName() + ")");
    if (const TransformPaintPropertyNode* scrollbar_paint_offset =
            paint_properties.ScrollbarPaintOffset())
      printer.AddPropertyNode(
          scrollbar_paint_offset,
          "ScrollbarPaintOffset (" + object.DebugName() + ")");
  }
};

template <>
class PropertyTreePrinterTraits<ClipPaintPropertyNode> {
 public:
  static void AddFrameViewProperties(
      const LocalFrameView& frame_view,
      PropertyTreePrinter<ClipPaintPropertyNode>& printer) {
    if (const ClipPaintPropertyNode* content_clip = frame_view.ContentClip())
      printer.AddPropertyNode(content_clip, "ContentClip (FrameView)");
  }

  static void AddObjectPaintProperties(
      const LayoutObject& object,
      const ObjectPaintProperties& paint_properties,
      PropertyTreePrinter<ClipPaintPropertyNode>& printer) {
    if (const ClipPaintPropertyNode* css_clip = paint_properties.CssClip())
      printer.AddPropertyNode(css_clip, "CssClip (" + object.DebugName() + ")");
    if (const ClipPaintPropertyNode* css_clip_fixed_position =
            paint_properties.CssClipFixedPosition())
      printer.AddPropertyNode(
          css_clip_fixed_position,
          "CssClipFixedPosition (" + object.DebugName() + ")");
    if (const ClipPaintPropertyNode* inner_border_radius_clip =
            paint_properties.InnerBorderRadiusClip())
      printer.AddPropertyNode(
          inner_border_radius_clip,
          "InnerBorderRadiusClip (" + object.DebugName() + ")");
    if (const ClipPaintPropertyNode* overflow_clip =
            paint_properties.OverflowClip())
      printer.AddPropertyNode(overflow_clip,
                              "OverflowClip (" + object.DebugName() + ")");
  }
};

template <>
class PropertyTreePrinterTraits<EffectPaintPropertyNode> {
 public:
  static void AddFrameViewProperties(
      const LocalFrameView& frame_view,
      PropertyTreePrinter<EffectPaintPropertyNode>& printer) {}

  static void AddObjectPaintProperties(
      const LayoutObject& object,
      const ObjectPaintProperties& paint_properties,
      PropertyTreePrinter<EffectPaintPropertyNode>& printer) {
    if (const EffectPaintPropertyNode* effect = paint_properties.Effect())
      printer.AddPropertyNode(effect, "Effect (" + object.DebugName() + ")");
  }
};

template <>
class PropertyTreePrinterTraits<ScrollPaintPropertyNode> {
 public:
  static void AddFrameViewProperties(
      const LocalFrameView& frame_view,
      PropertyTreePrinter<ScrollPaintPropertyNode>& printer) {
    if (const auto* scroll_translation = frame_view.ScrollTranslation()) {
      const auto* scroll_node = scroll_translation->ScrollNode();
      printer.AddPropertyNode(scroll_node, "Scroll (FrameView)");
    }
  }

  static void AddObjectPaintProperties(
      const LayoutObject& object,
      const ObjectPaintProperties& paint_properties,
      PropertyTreePrinter<ScrollPaintPropertyNode>& printer) {
    if (const auto* scroll_translation = paint_properties.ScrollTranslation()) {
      printer.AddPropertyNode(scroll_translation->ScrollNode(),
                              "Scroll (" + object.DebugName() + ")");
    }
  }
};

class PaintPropertyTreeGraphBuilder {
 public:
  PaintPropertyTreeGraphBuilder() {}

  void GenerateTreeGraph(const LocalFrameView& frame_view,
                         StringBuilder& string_builder) {
    layout_.str("");
    properties_.str("");
    owner_edges_.str("");
    properties_ << std::setprecision(2)
                << std::setiosflags(std::ios_base::fixed);

    WriteFrameViewNode(frame_view, nullptr);

    string_builder.Append("digraph {\n");
    string_builder.Append("graph [rankdir=BT];\n");
    string_builder.Append("subgraph cluster_layout {\n");
    std::string layout_str = layout_.str();
    string_builder.Append(layout_str.c_str(), layout_str.length());
    string_builder.Append("}\n");
    string_builder.Append("subgraph cluster_properties {\n");
    std::string properties_str = properties_.str();
    string_builder.Append(properties_str.c_str(), properties_str.length());
    string_builder.Append("}\n");
    std::string owners_str = owner_edges_.str();
    string_builder.Append(owners_str.c_str(), owners_str.length());
    string_builder.Append("}\n");
  }

 private:
  static String GetTagName(Node* n) {
    if (n->IsDocumentNode())
      return "";
    if (n->getNodeType() == Node::kCommentNode)
      return "COMMENT";
    return n->nodeName();
  }

  static void WriteParentEdge(const void* node,
                              const void* parent,
                              const char* color,
                              std::ostream& os) {
    os << "n" << node << " -> "
       << "n" << parent;
    if (color)
      os << " [color=" << color << "]";
    os << ";" << std::endl;
  }

  void WriteOwnerEdge(const void* node, const void* owner) {
    std::ostream& os = owner_edges_;
    os << "n" << owner << " -> "
       << "n" << node << " [color=" << layout_node_color_
       << ", arrowhead=dot, constraint=false];" << std::endl;
  }

  void WriteTransformEdge(const void* node, const void* transform) {
    std::ostream& os = properties_;
    os << "n" << node << " -> "
       << "n" << transform << " [color=" << transform_node_color_ << "];"
       << std::endl;
  }

  void WritePaintPropertyNode(const TransformPaintPropertyNode& node,
                              const void* owner,
                              const char* label) {
    std::ostream& os = properties_;
    os << "n" << &node << " [color=" << transform_node_color_
       << ", fontcolor=" << transform_node_color_ << ", shape=box, label=\""
       << label << "\\n";

    const FloatPoint3D& origin = node.Origin();
    os << "origin=(";
    os << origin.X() << "," << origin.Y() << "," << origin.Z() << ")\\n";

    os << "flattensInheritedTransform="
       << (node.FlattensInheritedTransform() ? "true" : "false") << "\\n";
    os << "renderingContextId=" << node.RenderingContextId() << "\\n";

    const TransformationMatrix& matrix = node.Matrix();
    os << "[" << std::setw(8) << matrix.M11() << "," << std::setw(8)
       << matrix.M12() << "," << std::setw(8) << matrix.M13() << ","
       << std::setw(8) << matrix.M14() << "\\n";
    os << std::setw(9) << matrix.M21() << "," << std::setw(8) << matrix.M22()
       << "," << std::setw(8) << matrix.M23() << "," << std::setw(8)
       << matrix.M24() << "\\n";
    os << std::setw(9) << matrix.M31() << "," << std::setw(8) << matrix.M32()
       << "," << std::setw(8) << matrix.M33() << "," << std::setw(8)
       << matrix.M34() << "\\n";
    os << std::setw(9) << matrix.M41() << "," << std::setw(8) << matrix.M42()
       << "," << std::setw(8) << matrix.M43() << "," << std::setw(8)
       << matrix.M44() << "]";

    TransformationMatrix::DecomposedType decomposition;
    if (!node.Matrix().Decompose(decomposition))
      os << "\\n(degenerate)";

    os << "\"];" << std::endl;

    if (owner)
      WriteOwnerEdge(&node, owner);
    if (node.Parent())
      WriteParentEdge(&node, node.Parent(), transform_node_color_, os);
  }

  void WritePaintPropertyNode(const ClipPaintPropertyNode& node,
                              const void* owner,
                              const char* label) {
    std::ostream& os = properties_;
    os << "n" << &node << " [color=" << clip_node_color_
       << ", fontcolor=" << clip_node_color_ << ", shape=box, label=\"" << label
       << "\\n";

    LayoutRect rect(node.ClipRect().Rect());
    if (IntRect(rect) == LayoutRect::InfiniteIntRect())
      os << "(infinite)";
    else
      os << "(" << rect.X() << ", " << rect.Y() << ", " << rect.Width() << ", "
         << rect.Height() << ")";
    os << "\"];" << std::endl;

    if (owner)
      WriteOwnerEdge(&node, owner);
    if (node.Parent())
      WriteParentEdge(&node, node.Parent(), clip_node_color_, os);
    if (node.LocalTransformSpace())
      WriteTransformEdge(&node, node.LocalTransformSpace());
  }

  void WritePaintPropertyNode(const EffectPaintPropertyNode& node,
                              const void* owner,
                              const char* label) {
    std::ostream& os = properties_;
    os << "n" << &node << " [shape=diamond, label=\"" << label << "\\n";
    os << "opacity=" << node.Opacity() << "\"];" << std::endl;

    if (owner)
      WriteOwnerEdge(&node, owner);
    if (node.Parent())
      WriteParentEdge(&node, node.Parent(), effect_node_color_, os);
  }

  void WritePaintPropertyNode(const ScrollPaintPropertyNode&,
                              const void*,
                              const char*) {
    // TODO(pdr): fill this out.
  }

  void WriteObjectPaintPropertyNodes(const LayoutObject& object) {
    const ObjectPaintProperties* properties =
        object.FirstFragment() ? object.FirstFragment()->PaintProperties()
                               : nullptr;
    if (!properties)
      return;
    const TransformPaintPropertyNode* paint_offset =
        properties->PaintOffsetTranslation();
    if (paint_offset)
      WritePaintPropertyNode(*paint_offset, &object, "paintOffset");
    const TransformPaintPropertyNode* transform = properties->Transform();
    if (transform)
      WritePaintPropertyNode(*transform, &object, "transform");
    const TransformPaintPropertyNode* perspective = properties->Perspective();
    if (perspective)
      WritePaintPropertyNode(*perspective, &object, "perspective");
    const TransformPaintPropertyNode* svg_local_to_border_box =
        properties->SvgLocalToBorderBoxTransform();
    if (svg_local_to_border_box)
      WritePaintPropertyNode(*svg_local_to_border_box, &object,
                             "svgLocalToBorderBox");
    const TransformPaintPropertyNode* scroll_translation =
        properties->ScrollTranslation();
    if (scroll_translation)
      WritePaintPropertyNode(*scroll_translation, &object, "scrollTranslation");
    const TransformPaintPropertyNode* scrollbar_paint_offset =
        properties->ScrollbarPaintOffset();
    if (scrollbar_paint_offset)
      WritePaintPropertyNode(*scrollbar_paint_offset, &object,
                             "scrollbarPaintOffset");
    const EffectPaintPropertyNode* effect = properties->Effect();
    if (effect)
      WritePaintPropertyNode(*effect, &object, "effect");
    const ClipPaintPropertyNode* css_clip = properties->CssClip();
    if (css_clip)
      WritePaintPropertyNode(*css_clip, &object, "cssClip");
    const ClipPaintPropertyNode* css_clip_fixed_position =
        properties->CssClipFixedPosition();
    if (css_clip_fixed_position)
      WritePaintPropertyNode(*css_clip_fixed_position, &object,
                             "cssClipFixedPosition");
    const ClipPaintPropertyNode* overflow_clip = properties->OverflowClip();
    if (overflow_clip) {
      if (const ClipPaintPropertyNode* inner_border_radius_clip =
              properties->InnerBorderRadiusClip())
        WritePaintPropertyNode(*inner_border_radius_clip, &object,
                               "innerBorderRadiusClip");
      WritePaintPropertyNode(*overflow_clip, &object, "overflowClip");
      if (object.IsLayoutView() && overflow_clip->Parent())
        WritePaintPropertyNode(*overflow_clip->Parent(), nullptr, "rootClip");
    }

    const auto* scroll =
        scroll_translation ? scroll_translation->ScrollNode() : nullptr;
    if (scroll)
      WritePaintPropertyNode(*scroll, &object, "scroll");
  }

  template <typename PropertyTreeNode>
  static const PropertyTreeNode* GetRoot(const PropertyTreeNode* node) {
    while (node && !node->IsRoot())
      node = node->Parent();
    return node;
  }

  void WriteFrameViewPaintPropertyNodes(const LocalFrameView& frame_view) {
    if (const auto* contents_state =
            frame_view.TotalPropertyTreeStateForContents()) {
      if (const auto* root = GetRoot(contents_state->Transform()))
        WritePaintPropertyNode(*root, &frame_view, "rootTransform");
      if (const auto* root = GetRoot(contents_state->Clip()))
        WritePaintPropertyNode(*root, &frame_view, "rootClip");
      if (const auto* root = GetRoot(contents_state->Effect()))
        WritePaintPropertyNode(*root, &frame_view, "rootEffect");
    }
    TransformPaintPropertyNode* pre_translation = frame_view.PreTranslation();
    if (pre_translation)
      WritePaintPropertyNode(*pre_translation, &frame_view, "preTranslation");
    TransformPaintPropertyNode* scroll_translation =
        frame_view.ScrollTranslation();
    if (scroll_translation)
      WritePaintPropertyNode(*scroll_translation, &frame_view,
                             "scrollTranslation");
    ClipPaintPropertyNode* content_clip = frame_view.ContentClip();
    if (content_clip)
      WritePaintPropertyNode(*content_clip, &frame_view, "contentClip");
    const auto* scroll =
        scroll_translation ? scroll_translation->ScrollNode() : nullptr;
    if (scroll)
      WritePaintPropertyNode(*scroll, &frame_view, "scroll");
  }

  void WriteLayoutObjectNode(const LayoutObject& object) {
    std::ostream& os = layout_;
    os << "n" << &object << " [color=" << layout_node_color_
       << ", fontcolor=" << layout_node_color_ << ", label=\""
       << object.GetName();
    Node* node = object.GetNode();
    if (node) {
      os << "\\n" << GetTagName(node).Utf8().data();
      if (node->IsElementNode() && ToElement(node)->HasID())
        os << "\\nid=" << ToElement(node)->GetIdAttribute().Utf8().data();
    }
    os << "\"];" << std::endl;
    const void* parent = object.IsLayoutView()
                             ? (const void*)ToLayoutView(object).GetFrameView()
                             : (const void*)object.Parent();
    WriteParentEdge(&object, parent, layout_node_color_, os);
    WriteObjectPaintPropertyNodes(object);
    for (const LayoutObject* child = object.SlowFirstChild(); child;
         child = child->NextSibling())
      WriteLayoutObjectNode(*child);
    if (object.IsLayoutEmbeddedContent()) {
      LocalFrameView* frame_view =
          ToLayoutEmbeddedContent(object).ChildFrameView();
      if (frame_view)
        WriteFrameViewNode(*frame_view, &object);
    }
  }

  void WriteFrameViewNode(const LocalFrameView& frame_view,
                          const void* parent) {
    std::ostream& os = layout_;
    os << "n" << &frame_view << " [color=" << layout_node_color_
       << ", fontcolor=" << layout_node_color_ << ", shape=doublecircle"
       << ", label=FrameView];" << std::endl;

    WriteFrameViewPaintPropertyNodes(frame_view);
    if (parent)
      WriteParentEdge(&frame_view, parent, layout_node_color_, os);
    LayoutView* layout_view = frame_view.GetLayoutView();
    if (layout_view)
      WriteLayoutObjectNode(*layout_view);
  }

 private:
  static const char* layout_node_color_;
  static const char* transform_node_color_;
  static const char* clip_node_color_;
  static const char* effect_node_color_;

  std::ostringstream layout_;
  std::ostringstream properties_;
  std::ostringstream owner_edges_;
};

const char* PaintPropertyTreeGraphBuilder::layout_node_color_ = "black";
const char* PaintPropertyTreeGraphBuilder::transform_node_color_ = "green";
const char* PaintPropertyTreeGraphBuilder::clip_node_color_ = "blue";
const char* PaintPropertyTreeGraphBuilder::effect_node_color_ = "black";

}  // namespace {
}  // namespace blink

CORE_EXPORT void showAllPropertyTrees(const blink::LocalFrameView& rootFrame) {
  showTransformPropertyTree(rootFrame);
  showClipPropertyTree(rootFrame);
  showEffectPropertyTree(rootFrame);
  showScrollPropertyTree(rootFrame);
}

void showTransformPropertyTree(const blink::LocalFrameView& rootFrame) {
  fprintf(stderr, "%s\n",
          transformPropertyTreeAsString(rootFrame).Utf8().data());
}

void showClipPropertyTree(const blink::LocalFrameView& rootFrame) {
  fprintf(stderr, "%s\n", clipPropertyTreeAsString(rootFrame).Utf8().data());
}

void showEffectPropertyTree(const blink::LocalFrameView& rootFrame) {
  fprintf(stderr, "%s\n", effectPropertyTreeAsString(rootFrame).Utf8().data());
}

void showScrollPropertyTree(const blink::LocalFrameView& rootFrame) {
  fprintf(stderr, "%s\n", scrollPropertyTreeAsString(rootFrame).Utf8().data());
}

String transformPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::PropertyTreePrinter<blink::TransformPaintPropertyNode>()
      .TreeAsString(rootFrame);
}

String clipPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::PropertyTreePrinter<blink::ClipPaintPropertyNode>()
      .TreeAsString(rootFrame);
}

String effectPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::PropertyTreePrinter<blink::EffectPaintPropertyNode>()
      .TreeAsString(rootFrame);
}

String scrollPropertyTreeAsString(const blink::LocalFrameView& rootFrame) {
  return blink::PropertyTreePrinter<blink::ScrollPaintPropertyNode>()
      .TreeAsString(rootFrame);
}

String paintPropertyTreeGraph(const blink::LocalFrameView& frameView) {
  blink::PaintPropertyTreeGraphBuilder builder;
  StringBuilder stringBuilder;
  builder.GenerateTreeGraph(frameView, stringBuilder);
  return stringBuilder.ToString();
}

#endif  // DCHECK_IS_ON()
