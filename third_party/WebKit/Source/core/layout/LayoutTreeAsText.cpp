/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/LayoutTreeAsText.h"

#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/PseudoElement.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/VisibleSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/html_names.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutDetailsMarker.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/LayoutFileUploadControl.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/LayoutListItem.h"
#include "core/layout/LayoutListMarker.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutView.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/line/InlineTextBox.h"
#include "core/layout/ng/layout_ng_list_item.h"
#include "core/layout/svg/LayoutSVGGradientStop.h"
#include "core/layout/svg/LayoutSVGImage.h"
#include "core/layout/svg/LayoutSVGInline.h"
#include "core/layout/svg/LayoutSVGInlineText.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/LayoutSVGShape.h"
#include "core/layout/svg/LayoutSVGText.h"
#include "core/layout/svg/SVGLayoutTreeAsText.h"
#include "core/page/PrintContext.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/compositing/CompositedLayerMapping.h"
#include "platform/LayoutUnit.h"
#include "platform/wtf/HexNumber.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/CharacterNames.h"

namespace blink {

using namespace HTMLNames;

static void PrintBorderStyle(TextStream& ts, const EBorderStyle border_style) {
  switch (border_style) {
    case EBorderStyle::kNone:
      ts << "none";
      break;
    case EBorderStyle::kHidden:
      ts << "hidden";
      break;
    case EBorderStyle::kInset:
      ts << "inset";
      break;
    case EBorderStyle::kGroove:
      ts << "groove";
      break;
    case EBorderStyle::kRidge:
      ts << "ridge";
      break;
    case EBorderStyle::kOutset:
      ts << "outset";
      break;
    case EBorderStyle::kDotted:
      ts << "dotted";
      break;
    case EBorderStyle::kDashed:
      ts << "dashed";
      break;
    case EBorderStyle::kSolid:
      ts << "solid";
      break;
    case EBorderStyle::kDouble:
      ts << "double";
      break;
  }

  ts << " ";
}

static String GetTagName(Node* n) {
  if (n->IsDocumentNode())
    return "";
  if (n->getNodeType() == Node::kCommentNode)
    return "COMMENT";
  return n->nodeName();
}

String QuoteAndEscapeNonPrintables(const String& s) {
  StringBuilder result;
  result.Append('"');
  for (unsigned i = 0; i != s.length(); ++i) {
    UChar c = s[i];
    if (c == '\\') {
      result.Append('\\');
      result.Append('\\');
    } else if (c == '"') {
      result.Append('\\');
      result.Append('"');
    } else if (c == '\n' || c == kNoBreakSpaceCharacter) {
      result.Append(' ');
    } else {
      if (c >= 0x20 && c < 0x7F) {
        result.Append(c);
      } else {
        result.Append('\\');
        result.Append('x');
        result.Append('{');
        HexNumber::AppendUnsignedAsHex(c, result);
        result.Append('}');
      }
    }
  }
  result.Append('"');
  return result.ToString();
}

TextStream& operator<<(TextStream& ts, const Color& c) {
  return ts << c.NameForLayoutTreeAsText();
}

void LayoutTreeAsText::WriteLayoutObject(TextStream& ts,
                                         const LayoutObject& o,
                                         LayoutAsTextBehavior behavior) {
  ts << o.DecoratedName();

  if (behavior & kLayoutAsTextShowAddresses)
    ts << " " << static_cast<const void*>(&o);

  if (o.Style() && o.Style()->ZIndex())
    ts << " zI: " << o.Style()->ZIndex();

  if (o.GetNode()) {
    String tag_name = GetTagName(o.GetNode());
    if (!tag_name.IsEmpty())
      ts << " {" << tag_name << "}";
  }

  LayoutRect rect = o.DebugRect();
  ts << " " << rect;

  if (!(o.IsText() && !o.IsBR())) {
    if (o.IsFileUploadControl())
      ts << " "
         << QuoteAndEscapeNonPrintables(
                ToLayoutFileUploadControl(&o)->FileTextValue());

    if (o.Parent()) {
      Color color = o.ResolveColor(CSSPropertyColor);
      if (o.Parent()->ResolveColor(CSSPropertyColor) != color)
        ts << " [color=" << color << "]";

      // Do not dump invalid or transparent backgrounds, since that is the
      // default.
      Color background_color = o.ResolveColor(CSSPropertyBackgroundColor);
      if (o.Parent()->ResolveColor(CSSPropertyBackgroundColor) !=
              background_color &&
          background_color.Rgb())
        ts << " [bgcolor=" << background_color << "]";

      Color text_fill_color = o.ResolveColor(CSSPropertyWebkitTextFillColor);
      if (o.Parent()->ResolveColor(CSSPropertyWebkitTextFillColor) !=
              text_fill_color &&
          text_fill_color != color && text_fill_color.Rgb())
        ts << " [textFillColor=" << text_fill_color << "]";

      Color text_stroke_color =
          o.ResolveColor(CSSPropertyWebkitTextStrokeColor);
      if (o.Parent()->ResolveColor(CSSPropertyWebkitTextStrokeColor) !=
              text_stroke_color &&
          text_stroke_color != color && text_stroke_color.Rgb())
        ts << " [textStrokeColor=" << text_stroke_color << "]";

      if (o.Parent()->Style()->TextStrokeWidth() !=
              o.Style()->TextStrokeWidth() &&
          o.Style()->TextStrokeWidth() > 0)
        ts << " [textStrokeWidth=" << o.Style()->TextStrokeWidth() << "]";
    }

    if (!o.IsBoxModelObject())
      return;

    const LayoutBoxModelObject& box = ToLayoutBoxModelObject(o);
    if (box.BorderTop() || box.BorderRight() || box.BorderBottom() ||
        box.BorderLeft()) {
      ts << " [border:";

      BorderValue prev_border = o.Style()->BorderTop();
      if (!box.BorderTop()) {
        ts << " none";
      } else {
        ts << " (" << box.BorderTop() << "px ";
        PrintBorderStyle(ts, o.Style()->BorderTopStyle());
        ts << o.ResolveColor(CSSPropertyBorderTopColor) << ")";
      }

      if (!o.Style()->BorderRightEquals(prev_border)) {
        prev_border = o.Style()->BorderRight();
        if (!box.BorderRight()) {
          ts << " none";
        } else {
          ts << " (" << box.BorderRight() << "px ";
          PrintBorderStyle(ts, o.Style()->BorderRightStyle());
          ts << o.ResolveColor(CSSPropertyBorderRightColor) << ")";
        }
      }

      if (!o.Style()->BorderBottomEquals(prev_border)) {
        prev_border = box.Style()->BorderBottom();
        if (!box.BorderBottom()) {
          ts << " none";
        } else {
          ts << " (" << box.BorderBottom() << "px ";
          PrintBorderStyle(ts, o.Style()->BorderBottomStyle());
          ts << o.ResolveColor(CSSPropertyBorderBottomColor) << ")";
        }
      }

      if (!o.Style()->BorderLeftEquals(prev_border)) {
        prev_border = o.Style()->BorderLeft();
        if (!box.BorderLeft()) {
          ts << " none";
        } else {
          ts << " (" << box.BorderLeft() << "px ";
          PrintBorderStyle(ts, o.Style()->BorderLeftStyle());
          ts << o.ResolveColor(CSSPropertyBorderLeftColor) << ")";
        }
      }

      ts << "]";
    }
  }

  if (o.IsTableCell()) {
    const LayoutTableCell& c = ToLayoutTableCell(o);
    ts << " [r=" << c.RowIndex() << " c=" << c.AbsoluteColumnIndex()
       << " rs=" << c.RowSpan() << " cs=" << c.ColSpan() << "]";
  }

  if (o.IsDetailsMarker()) {
    ts << ": ";
    switch (ToLayoutDetailsMarker(&o)->GetOrientation()) {
      case LayoutDetailsMarker::kLeft:
        ts << "left";
        break;
      case LayoutDetailsMarker::kRight:
        ts << "right";
        break;
      case LayoutDetailsMarker::kUp:
        ts << "up";
        break;
      case LayoutDetailsMarker::kDown:
        ts << "down";
        break;
    }
  }

  if (o.IsListMarker()) {
    String text = ToLayoutListMarker(o).GetText();
    if (!text.IsEmpty()) {
      if (text.length() != 1) {
        text = QuoteAndEscapeNonPrintables(text);
      } else {
        switch (text[0]) {
          case kBulletCharacter:
            text = "bullet";
            break;
          case kBlackSquareCharacter:
            text = "black square";
            break;
          case kWhiteBulletCharacter:
            text = "white bullet";
            break;
          default:
            text = QuoteAndEscapeNonPrintables(text);
        }
      }
      ts << ": " << text;
    }
  }

  if (behavior & kLayoutAsTextShowIDAndClass) {
    Node* node = o.GetNode();
    if (node && node->IsElementNode()) {
      Element& element = ToElement(*node);
      if (element.HasID())
        ts << " id=\"" + element.GetIdAttribute() + "\"";

      if (element.HasClass()) {
        ts << " class=\"";
        for (size_t i = 0; i < element.ClassNames().size(); ++i) {
          if (i > 0)
            ts << " ";
          ts << element.ClassNames()[i];
        }
        ts << "\"";
      }
    }
  }

  if (behavior & kLayoutAsTextShowLayoutState) {
    bool needs_layout = o.SelfNeedsLayout() ||
                        o.NeedsPositionedMovementLayout() ||
                        o.PosChildNeedsLayout() || o.NormalChildNeedsLayout();
    if (needs_layout)
      ts << " (needs layout:";

    bool have_previous = false;
    if (o.SelfNeedsLayout()) {
      ts << " self";
      have_previous = true;
    }

    if (o.NeedsPositionedMovementLayout()) {
      if (have_previous)
        ts << ",";
      have_previous = true;
      ts << " positioned movement";
    }

    if (o.NormalChildNeedsLayout()) {
      if (have_previous)
        ts << ",";
      have_previous = true;
      ts << " child";
    }

    if (o.PosChildNeedsLayout()) {
      if (have_previous)
        ts << ",";
      ts << " positioned child";
    }

    if (needs_layout)
      ts << ")";
  }
}

static void WriteInlineBox(TextStream& ts, const InlineBox& box, int indent) {
  WriteIndent(ts, indent);
  ts << "+ ";
  ts << box.BoxName() << " {" << box.GetLineLayoutItem().DebugName() << "}"
     << " pos=(" << box.X() << "," << box.Y() << ")"
     << " size=(" << box.Width() << "," << box.Height() << ")"
     << " baseline=" << box.BaselinePosition(kAlphabeticBaseline) << "/"
     << box.BaselinePosition(kIdeographicBaseline);
}

static void WriteInlineTextBox(TextStream& ts,
                               const InlineTextBox& text_box,
                               int indent) {
  WriteInlineBox(ts, text_box, indent);
  String value = text_box.GetText();
  value.Replace('\\', "\\\\");
  value.Replace('\n', "\\n");
  value.Replace('"', "\\\"");
  ts << " range=(" << text_box.Start() << ","
     << (text_box.Start() + text_box.Len()) << ")"
     << " \"" << value << "\"";
}

static void WriteInlineFlowBox(TextStream& ts,
                               const InlineFlowBox& root_box,
                               int indent) {
  WriteInlineBox(ts, root_box, indent);
  ts << "\n";
  for (const InlineBox* box = root_box.FirstChild(); box;
       box = box->NextOnLine()) {
    if (box->IsInlineFlowBox()) {
      WriteInlineFlowBox(ts, static_cast<const InlineFlowBox&>(*box),
                         indent + 1);
      continue;
    }
    if (box->IsInlineTextBox())
      WriteInlineTextBox(ts, static_cast<const InlineTextBox&>(*box),
                         indent + 1);
    else
      WriteInlineBox(ts, *box, indent + 1);
    ts << "\n";
  }
}

void LayoutTreeAsText::WriteLineBoxTree(TextStream& ts,
                                        const LayoutBlockFlow& o,
                                        int indent) {
  for (const InlineFlowBox* root_box = o.FirstLineBox(); root_box;
       root_box = root_box->NextLineBox()) {
    WriteInlineFlowBox(ts, *root_box, indent);
  }
}

static void WriteTextRun(TextStream& ts,
                         const LayoutText& o,
                         const InlineTextBox& run) {
  // FIXME: For now use an "enclosingIntRect" model for x, y and logicalWidth,
  // although this makes it harder to detect any changes caused by the
  // conversion to floating point. :(
  int x = run.X().ToInt();
  int y = run.Y().ToInt();
  int logical_width = (run.X() + run.LogicalWidth()).Ceil() - x;

  // FIXME: Table cell adjustment is temporary until results can be updated.
  if (o.ContainingBlock()->IsTableCell())
    y -= ToLayoutTableCell(o.ContainingBlock())->IntrinsicPaddingBefore();

  ts << "text run at (" << x << "," << y << ") width " << logical_width;
  if (!run.IsLeftToRightDirection() || run.DirOverride()) {
    ts << (!run.IsLeftToRightDirection() ? " RTL" : " LTR");
    if (run.DirOverride())
      ts << " override";
  }
  ts << ": "
     << QuoteAndEscapeNonPrintables(
            String(o.GetText()).Substring(run.Start(), run.Len()));
  if (run.HasHyphen())
    ts << " + hyphen string "
       << QuoteAndEscapeNonPrintables(o.Style()->HyphenString());
  ts << "\n";
}

void Write(TextStream& ts,
           const LayoutObject& o,
           int indent,
           LayoutAsTextBehavior behavior) {
  if (o.IsSVGShape()) {
    Write(ts, ToLayoutSVGShape(o), indent);
    return;
  }
  if (o.IsSVGGradientStop()) {
    WriteSVGGradientStop(ts, ToLayoutSVGGradientStop(o), indent);
    return;
  }
  if (o.IsSVGResourceContainer()) {
    WriteSVGResourceContainer(ts, o, indent);
    return;
  }
  if (o.IsSVGContainer()) {
    WriteSVGContainer(ts, o, indent);
    return;
  }
  if (o.IsSVGRoot()) {
    Write(ts, ToLayoutSVGRoot(o), indent);
    return;
  }
  if (o.IsSVGText()) {
    WriteSVGText(ts, ToLayoutSVGText(o), indent);
    return;
  }
  if (o.IsSVGInline()) {
    WriteSVGInline(ts, ToLayoutSVGInline(o), indent);
    return;
  }
  if (o.IsSVGInlineText()) {
    WriteSVGInlineText(ts, ToLayoutSVGInlineText(o), indent);
    return;
  }
  if (o.IsSVGImage()) {
    WriteSVGImage(ts, ToLayoutSVGImage(o), indent);
    return;
  }

  WriteIndent(ts, indent);

  LayoutTreeAsText::WriteLayoutObject(ts, o, behavior);
  ts << "\n";

  if ((behavior & kLayoutAsTextShowLineTrees) && o.IsLayoutBlockFlow()) {
    LayoutTreeAsText::WriteLineBoxTree(ts, ToLayoutBlockFlow(o), indent + 1);
  }

  if (o.IsText() && !o.IsBR()) {
    const LayoutText& text = ToLayoutText(o);
    for (InlineTextBox* box = text.FirstTextBox(); box;
         box = box->NextTextBox()) {
      WriteIndent(ts, indent + 1);
      WriteTextRun(ts, text, *box);
    }
  }

  for (LayoutObject* child = o.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->HasLayer())
      continue;
    Write(ts, *child, indent + 1, behavior);
  }

  if (o.IsLayoutEmbeddedContent()) {
    LocalFrameView* frame_view = ToLayoutEmbeddedContent(o).ChildFrameView();
    if (frame_view) {
      LayoutViewItem root_item = frame_view->GetLayoutViewItem();
      if (!root_item.IsNull()) {
        root_item.UpdateStyleAndLayout();
        if (auto* layer = root_item.Layer()) {
          LayoutTreeAsText::WriteLayers(
              ts, layer, layer, layer->RectIgnoringNeedsPositionUpdate(),
              indent + 1, behavior);
        }
      }
    }
  }
}

enum LayerPaintPhase {
  kLayerPaintPhaseAll = 0,
  kLayerPaintPhaseBackground = -1,
  kLayerPaintPhaseForeground = 1
};

static void Write(TextStream& ts,
                  PaintLayer& layer,
                  const LayoutRect& layer_bounds,
                  const LayoutRect& background_clip_rect,
                  const LayoutRect& clip_rect,
                  LayerPaintPhase paint_phase = kLayerPaintPhaseAll,
                  int indent = 0,
                  LayoutAsTextBehavior behavior = kLayoutAsTextBehaviorNormal,
                  const PaintLayer* marked_layer = nullptr) {
  IntRect adjusted_layout_bounds = PixelSnappedIntRect(layer_bounds);
  IntRect adjusted_layout_bounds_with_scrollbars = adjusted_layout_bounds;
  IntRect adjusted_background_clip_rect =
      PixelSnappedIntRect(background_clip_rect);
  IntRect adjusted_clip_rect = PixelSnappedIntRect(clip_rect);

  bool report_frame_scroll_info =
      layer.GetLayoutObject().IsLayoutView() &&
      !RuntimeEnabledFeatures::RootLayerScrollingEnabled();

  if (report_frame_scroll_info) {
    LayoutView& layout_view = ToLayoutView(layer.GetLayoutObject());

    adjusted_layout_bounds_with_scrollbars.SetWidth(
        layout_view.ViewWidth(kIncludeScrollbars));
    adjusted_layout_bounds_with_scrollbars.SetHeight(
        layout_view.ViewHeight(kIncludeScrollbars));
  }

  if (marked_layer)
    ts << (marked_layer == &layer ? "*" : " ");

  WriteIndent(ts, indent);

  if (layer.GetLayoutObject().Style()->Visibility() == EVisibility::kHidden)
    ts << "hidden ";

  ts << "layer ";

  if (behavior & kLayoutAsTextShowAddresses)
    ts << static_cast<const void*>(&layer) << " ";

  ts << adjusted_layout_bounds_with_scrollbars;

  if (!adjusted_layout_bounds.IsEmpty()) {
    if (!adjusted_background_clip_rect.Contains(adjusted_layout_bounds))
      ts << " backgroundClip " << adjusted_background_clip_rect;
    if (!adjusted_clip_rect.Contains(adjusted_layout_bounds_with_scrollbars))
      ts << " clip " << adjusted_clip_rect;
  }
  if (layer.IsTransparent())
    ts << " transparent";

  if (layer.GetLayoutObject().HasOverflowClip() || report_frame_scroll_info) {
    ScrollableArea* scrollable_area;
    if (report_frame_scroll_info)
      scrollable_area = ToLayoutView(layer.GetLayoutObject()).GetFrameView();
    else
      scrollable_area = layer.GetScrollableArea();

    ScrollOffset adjusted_scroll_offset =
        scrollable_area->GetScrollOffset() +
        ToFloatSize(scrollable_area->ScrollOrigin());
    if (adjusted_scroll_offset.Width())
      ts << " scrollX " << adjusted_scroll_offset.Width();
    if (adjusted_scroll_offset.Height())
      ts << " scrollY " << adjusted_scroll_offset.Height();
    if (layer.GetLayoutBox() &&
        layer.GetLayoutBox()->PixelSnappedClientWidth() !=
            layer.GetLayoutBox()->PixelSnappedScrollWidth())
      ts << " scrollWidth " << layer.GetLayoutBox()->PixelSnappedScrollWidth();
    if (layer.GetLayoutBox() &&
        layer.GetLayoutBox()->PixelSnappedClientHeight() !=
            layer.GetLayoutBox()->PixelSnappedScrollHeight())
      ts << " scrollHeight "
         << layer.GetLayoutBox()->PixelSnappedScrollHeight();
  }

  if (paint_phase == kLayerPaintPhaseBackground)
    ts << " layerType: background only";
  else if (paint_phase == kLayerPaintPhaseForeground)
    ts << " layerType: foreground only";

  if (layer.GetLayoutObject().Style()->HasBlendMode()) {
    ts << " blendMode: "
       << CompositeOperatorName(kCompositeSourceOver,
                                layer.GetLayoutObject().Style()->BlendMode());
  }

  if (behavior & kLayoutAsTextShowCompositedLayers) {
    if (layer.HasCompositedLayerMapping()) {
      ts << " (composited, bounds="
         << layer.GetCompositedLayerMapping()->CompositedBounds()
         << ", drawsContent="
         << layer.GetCompositedLayerMapping()
                ->MainGraphicsLayer()
                ->DrawsContent()
         << (layer.ShouldIsolateCompositedDescendants()
                 ? ", isolatesCompositedBlending"
                 : "")
         << ")";
    }
  }

  ts << "\n";

  if (paint_phase != kLayerPaintPhaseBackground)
    Write(ts, layer.GetLayoutObject(), indent + 1, behavior);
}

static Vector<PaintLayerStackingNode*> NormalFlowListFor(
    PaintLayerStackingNode* node) {
  PaintLayerStackingNodeIterator it(*node, kNormalFlowChildren);
  Vector<PaintLayerStackingNode*> vector;
  while (PaintLayerStackingNode* normal_flow_child = it.Next())
    vector.push_back(normal_flow_child);
  return vector;
}

void LayoutTreeAsText::WriteLayers(TextStream& ts,
                                   const PaintLayer* root_layer,
                                   PaintLayer* layer,
                                   const LayoutRect& paint_rect,
                                   int indent,
                                   LayoutAsTextBehavior behavior,
                                   const PaintLayer* marked_layer) {
  // Calculate the clip rects we should use.
  LayoutRect layer_bounds;
  ClipRect damage_rect, clip_rect_to_apply;
  layer->Clipper(PaintLayer::kDoNotUseGeometryMapper)
      .CalculateRects(ClipRectsContext(root_layer, kUncachedClipRects), nullptr,
                      paint_rect, layer_bounds, damage_rect,
                      clip_rect_to_apply);

  // Ensure our lists are up to date.
  layer->StackingNode()->UpdateLayerListsIfNeeded();

  LayoutPoint offset_from_root;
  layer->ConvertToLayerCoords(root_layer, offset_from_root);
  bool should_paint =
      (behavior & kLayoutAsTextShowAllLayers)
          ? true
          : layer->IntersectsDamageRect(layer_bounds, damage_rect.Rect(),
                                        offset_from_root);

  if (layer->GetLayoutObject().IsLayoutEmbeddedContent() &&
      ToLayoutEmbeddedContent(layer->GetLayoutObject()).IsThrottledFrameView())
    should_paint = false;

#if DCHECK_IS_ON()
  if (layer->NeedsPositionUpdate()) {
    WriteIndent(ts, indent);
    ts << " NEEDS POSITION UPDATE\n";
  }
#endif

  Vector<PaintLayerStackingNode*>* neg_list =
      layer->StackingNode()->NegZOrderList();
  bool paints_background_separately = neg_list && neg_list->size() > 0;
  if (should_paint && paints_background_separately)
    Write(ts, *layer, layer_bounds, damage_rect.Rect(),
          clip_rect_to_apply.Rect(), kLayerPaintPhaseBackground, indent,
          behavior, marked_layer);

  if (neg_list) {
    int curr_indent = indent;
    if (behavior & kLayoutAsTextShowLayerNesting) {
      WriteIndent(ts, indent);
      ts << " negative z-order list(" << neg_list->size() << ")\n";
      ++curr_indent;
    }
    for (unsigned i = 0; i != neg_list->size(); ++i)
      WriteLayers(ts, root_layer, neg_list->at(i)->Layer(), paint_rect,
                  curr_indent, behavior, marked_layer);
  }

  if (should_paint)
    Write(ts, *layer, layer_bounds, damage_rect.Rect(),
          clip_rect_to_apply.Rect(),
          paints_background_separately ? kLayerPaintPhaseForeground
                                       : kLayerPaintPhaseAll,
          indent, behavior, marked_layer);

  Vector<PaintLayerStackingNode*> normal_flow_list =
      NormalFlowListFor(layer->StackingNode());
  if (!normal_flow_list.IsEmpty()) {
    int curr_indent = indent;
    if (behavior & kLayoutAsTextShowLayerNesting) {
      WriteIndent(ts, indent);
      ts << " normal flow list(" << normal_flow_list.size() << ")\n";
      ++curr_indent;
    }
    for (unsigned i = 0; i != normal_flow_list.size(); ++i)
      WriteLayers(ts, root_layer, normal_flow_list.at(i)->Layer(), paint_rect,
                  curr_indent, behavior, marked_layer);
  }

  if (Vector<PaintLayerStackingNode*>* pos_list =
          layer->StackingNode()->PosZOrderList()) {
    int curr_indent = indent;
    if (behavior & kLayoutAsTextShowLayerNesting) {
      WriteIndent(ts, indent);
      ts << " positive z-order list(" << pos_list->size() << ")\n";
      ++curr_indent;
    }
    for (unsigned i = 0; i != pos_list->size(); ++i)
      WriteLayers(ts, root_layer, pos_list->at(i)->Layer(), paint_rect,
                  curr_indent, behavior, marked_layer);
  }
}

static String NodePosition(Node* node) {
  StringBuilder result;

  Element* body = node->GetDocument().body();
  Node* parent;
  for (Node* n = node; n; n = parent) {
    parent = n->ParentOrShadowHostNode();
    if (n != node)
      result.Append(" of ");
    if (parent) {
      if (body && n == body) {
        // We don't care what offset body may be in the document.
        result.Append("body");
        break;
      }
      if (n->IsShadowRoot()) {
        result.Append('{');
        result.Append(GetTagName(n));
        result.Append('}');
      } else {
        result.Append("child ");
        result.AppendNumber(n->NodeIndex());
        result.Append(" {");
        result.Append(GetTagName(n));
        result.Append('}');
      }
    } else {
      result.Append("document");
    }
  }

  return result.ToString();
}

static void WriteSelection(TextStream& ts, const LayoutObject* o) {
  Node* n = o->GetNode();
  if (!n || !n->IsDocumentNode())
    return;

  Document* doc = ToDocument(n);
  LocalFrame* frame = doc->GetFrame();
  if (!frame)
    return;

  const VisibleSelection& selection =
      frame->Selection().ComputeVisibleSelectionInDOMTree();
  if (selection.IsCaret()) {
    ts << "caret: position " << selection.Start().ComputeEditingOffset()
       << " of " << NodePosition(selection.Start().AnchorNode());
    if (selection.Affinity() == TextAffinity::kUpstream)
      ts << " (upstream affinity)";
    ts << "\n";
  } else if (selection.IsRange()) {
    ts << "selection start: position "
       << selection.Start().ComputeEditingOffset() << " of "
       << NodePosition(selection.Start().AnchorNode()) << "\n"
       << "selection end:   position " << selection.End().ComputeEditingOffset()
       << " of " << NodePosition(selection.End().AnchorNode()) << "\n";
  }
}

static String ExternalRepresentation(LayoutBox* layout_object,
                                     LayoutAsTextBehavior behavior,
                                     const PaintLayer* marked_layer = nullptr) {
  TextStream ts;
  if (!layout_object->HasLayer())
    return ts.Release();

  PaintLayer* layer = layout_object->Layer();
  LayoutTreeAsText::WriteLayers(ts, layer, layer,
                                layer->RectIgnoringNeedsPositionUpdate(), 0,
                                behavior, marked_layer);
  WriteSelection(ts, layout_object);
  return ts.Release();
}

String ExternalRepresentation(LocalFrame* frame,
                              LayoutAsTextBehavior behavior,
                              const PaintLayer* marked_layer) {
  if (!(behavior & kLayoutAsTextDontUpdateLayout))
    frame->GetDocument()->UpdateStyleAndLayout();

  LayoutObject* layout_object = frame->ContentLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return String();

  PrintContext print_context(frame);
  bool is_text_printing_mode = !!(behavior & kLayoutAsTextPrintingMode);
  if (is_text_printing_mode) {
    FloatSize size(ToLayoutBox(layout_object)->Size());
    print_context.BeginPrintMode(size.Width(), size.Height());
  }

  String representation = ExternalRepresentation(ToLayoutBox(layout_object),
                                                 behavior, marked_layer);
  if (is_text_printing_mode)
    print_context.EndPrintMode();
  return representation;
}

String ExternalRepresentation(Element* element, LayoutAsTextBehavior behavior) {
  // Doesn't support printing mode.
  DCHECK(!(behavior & kLayoutAsTextPrintingMode));
  if (!(behavior & kLayoutAsTextDontUpdateLayout))
    element->GetDocument().UpdateStyleAndLayout();

  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return String();

  return ExternalRepresentation(ToLayoutBox(layout_object),
                                behavior | kLayoutAsTextShowAllLayers);
}

static void WriteCounterValuesFromChildren(TextStream& stream,
                                           LayoutObject* parent,
                                           bool& is_first_counter) {
  for (LayoutObject* child = parent->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (child->IsCounter()) {
      if (!is_first_counter)
        stream << " ";
      is_first_counter = false;
      String str(ToLayoutText(child)->GetText());
      stream << str;
    }
  }
}

String CounterValueForElement(Element* element) {
  element->GetDocument().UpdateStyleAndLayout();
  TextStream stream;
  bool is_first_counter = true;
  // The counter layoutObjects should be children of :before or :after
  // pseudo-elements.
  if (LayoutObject* before =
          element->PseudoElementLayoutObject(kPseudoIdBefore))
    WriteCounterValuesFromChildren(stream, before, is_first_counter);
  if (LayoutObject* after = element->PseudoElementLayoutObject(kPseudoIdAfter))
    WriteCounterValuesFromChildren(stream, after, is_first_counter);
  return stream.Release();
}

String MarkerTextForListItem(Element* element) {
  element->GetDocument().UpdateStyleAndLayout();

  LayoutObject* layout_object = element->GetLayoutObject();
  if (layout_object) {
    if (layout_object->IsListItem())
      return ToLayoutListItem(layout_object)->MarkerText();
    if (layout_object->IsLayoutNGListItem())
      return ToLayoutNGListItem(layout_object)->MarkerTextWithoutSuffix();
  }
  return String();
}

}  // namespace blink
