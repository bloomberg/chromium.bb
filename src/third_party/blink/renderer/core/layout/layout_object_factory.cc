// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_object_factory.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_deprecated_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_fieldset.h"
#include "third_party/blink/renderer/core/layout/layout_file_upload_control.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/core/layout/layout_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_list_item.h"
#include "third_party/blink/renderer/core/layout/layout_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/layout_table_caption.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_fieldset.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_grid.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_progress.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_ruby_as_block.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_inside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_outside_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_caption.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_cell_legacy.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

namespace {

inline Element* GetElementForLayoutObject(Node& node) {
  if (auto* element = DynamicTo<Element>(node))
    return element;
  // If |node| is a Document, the layout object is going to be anonymous.
  DCHECK(node.IsDocumentNode());
  return nullptr;
}

template <typename BaseType, typename NGType, typename LegacyType = BaseType>
inline BaseType* CreateObject(Node& node,
                              const ComputedStyle& style,
                              LegacyLayout legacy,
                              bool disable_ng_for_type = false) {
  Element* element = GetElementForLayoutObject(node);
  bool force_legacy = false;

  // If no reason has been found for disabling NG for this particular type,
  // check if the NG feature is enabled at all, before considering creating an
  // NG object.
  if (!disable_ng_for_type && RuntimeEnabledFeatures::LayoutNGEnabled()) {
    // The last thing to check is whether we should force legacy layout. This
    // happens when the NG feature is enabled for the object in question, but
    // we're dealing with something that isn't implemented in NG yet (such as
    // editing or multicol). We then need to force legacy layout for the entire
    // subtree.
    force_legacy = legacy == LegacyLayout::kForce;

    if (!force_legacy)
      return new NGType(element);
  }
  BaseType* new_object = new LegacyType(element);
  if (force_legacy)
    new_object->SetForceLegacyLayout();
  return new_object;
}

}  // anonymous namespace

LayoutBlockFlow* LayoutObjectFactory::CreateBlockFlow(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (style.Display() == EDisplay::kListItem) {
    // Create a LayoutBlockFlow with a list marker
    return CreateObject<LayoutBlockFlow, LayoutNGListItem, LayoutListItem>(
        node, style, legacy);
  }

  // Create a plain LayoutBlockFlow
  return CreateObject<LayoutBlockFlow, LayoutNGBlockFlow>(node, style, legacy);
}

// static
LayoutBlock* LayoutObjectFactory::CreateBlockForLineClamp(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  DCHECK(RuntimeEnabledFeatures::BlockFlowHandlesWebkitLineClampEnabled());
  return CreateObject<LayoutBlock, LayoutNGBlockFlow,
                      LayoutDeprecatedFlexibleBox>(node, style, legacy);
}

LayoutBlock* LayoutObjectFactory::CreateFlexibleBox(Node& node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  bool disable_ng_for_type = !RuntimeEnabledFeatures::LayoutNGFlexBoxEnabled();
  if (disable_ng_for_type)
    UseCounter::Count(node.GetDocument(), WebFeature::kLegacyLayoutByFlexBox);
  return CreateObject<LayoutBlock, LayoutNGFlexibleBox, LayoutFlexibleBox>(
      node, style, legacy, disable_ng_for_type);
}

LayoutBlock* LayoutObjectFactory::CreateGrid(Node& node,
                                             const ComputedStyle& style,
                                             LegacyLayout legacy) {
  bool disable_ng_for_type = !RuntimeEnabledFeatures::LayoutNGGridEnabled();
  if (disable_ng_for_type)
    UseCounter::Count(node.GetDocument(), WebFeature::kLegacyLayoutByGrid);
  return CreateObject<LayoutBlock, LayoutNGGrid, LayoutGrid>(
      node, style, legacy, disable_ng_for_type);
}

LayoutBlock* LayoutObjectFactory::CreateMath(Node& node,
                                             const ComputedStyle& style,
                                             LegacyLayout legacy) {
  bool disable_ng_for_type = !RuntimeEnabledFeatures::MathMLCoreEnabled();
  return CreateObject<LayoutBlock, LayoutNGMathMLBlock, LayoutBlockFlow>(
      node, style, legacy, disable_ng_for_type);
}

LayoutObject* LayoutObjectFactory::CreateListMarker(Node& node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  const Node* parent = node.parentNode();
  const ComputedStyle* parent_style = parent->GetComputedStyle();
  bool is_inside =
      parent_style->ListStylePosition() == EListStylePosition::kInside ||
      (IsA<HTMLLIElement>(parent) && !parent_style->IsInsideListElement());
  if (is_inside) {
    return CreateObject<LayoutObject, LayoutNGInsideListMarker,
                        LayoutInsideListMarker>(node, style, legacy);
  }
  return CreateObject<LayoutObject, LayoutNGOutsideListMarker,
                      LayoutOutsideListMarker>(node, style, legacy);
}

LayoutTableCaption* LayoutObjectFactory::CreateTableCaption(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutTableCaption, LayoutNGTableCaption>(node, style,
                                                                legacy);
}

LayoutTableCell* LayoutObjectFactory::CreateTableCell(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutTableCell, LayoutNGTableCellLegacy>(node, style,
                                                                legacy);
}

LayoutBlock* LayoutObjectFactory::CreateFieldset(Node& node,
                                                 const ComputedStyle& style,
                                                 LegacyLayout legacy) {
  bool disable_ng_for_type = !RuntimeEnabledFeatures::LayoutNGFieldsetEnabled();
  if (disable_ng_for_type)
    UseCounter::Count(node.GetDocument(), WebFeature::kLegacyLayoutByFieldSet);
  return CreateObject<LayoutBlock, LayoutNGFieldset, LayoutFieldset>(
      node, style, legacy, disable_ng_for_type);
}

LayoutBlockFlow* LayoutObjectFactory::CreateFileUploadControl(
    Node& node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutBlockFlow, LayoutNGBlockFlow,
                      LayoutFileUploadControl>(node, style, legacy);
}

LayoutText* LayoutObjectFactory::CreateText(Node* node,
                                            scoped_refptr<StringImpl> str,
                                            LegacyLayout legacy) {
  bool force_legacy = false;
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    force_legacy = legacy == LegacyLayout::kForce;
    if (!force_legacy)
      return new LayoutNGText(node, str);
  }
  LayoutText* layout_text = new LayoutText(node, str);
  if (force_legacy)
    layout_text->SetForceLegacyLayout();
  return layout_text;
}

LayoutTextFragment* LayoutObjectFactory::CreateTextFragment(
    Node* node,
    StringImpl* str,
    int start_offset,
    int length,
    LegacyLayout legacy) {
  bool force_legacy = false;
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    force_legacy = legacy == LegacyLayout::kForce;
    if (!force_legacy)
      return new LayoutNGTextFragment(node, str, start_offset, length);
  }
  LayoutTextFragment* layout_text_fragment =
      new LayoutTextFragment(node, str, start_offset, length);
  if (force_legacy)
    layout_text_fragment->SetForceLegacyLayout();
  return layout_text_fragment;
}

LayoutProgress* LayoutObjectFactory::CreateProgress(Node* node,
                                                    const ComputedStyle& style,
                                                    LegacyLayout legacy) {
  return CreateObject<LayoutProgress, LayoutNGProgress>(*node, style, legacy);
}

LayoutRubyAsBlock* LayoutObjectFactory::CreateRubyAsBlock(
    Node* node,
    const ComputedStyle& style,
    LegacyLayout legacy) {
  return CreateObject<LayoutRubyAsBlock, LayoutNGRubyAsBlock>(*node, style,
                                                              legacy);
}

}  // namespace blink
