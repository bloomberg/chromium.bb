/*
 * Copyright (C) 2007, 2008, 2009 Apple Computer, Inc.
 * Copyright (C) 2010, 2011 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/editing_style_utilities.h"

#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/editing_style.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"

namespace blink {

using namespace cssvalue;

namespace {

Position AdjustedSelectionStartForStyleComputation(const Position& position) {
  // This function is used by range style computations to avoid bugs like:
  // <rdar://problem/4017641> REGRESSION (Mail): you can only bold/unbold a
  // selection starting from end of line once
  // It is important to skip certain irrelevant content at the start of the
  // selection, so we do not wind up with a spurious "mixed" style.

  VisiblePosition visible_position = CreateVisiblePosition(position);
  if (visible_position.IsNull())
    return Position();

  // if the selection starts just before a paragraph break, skip over it
  if (IsEndOfParagraph(visible_position)) {
    return MostForwardCaretPosition(
        NextPositionOf(visible_position).DeepEquivalent());
  }

  // otherwise, make sure to be at the start of the first selected node,
  // instead of possibly at the end of the last node before the selection
  return MostForwardCaretPosition(visible_position.DeepEquivalent());
}

}  // anonymous namespace

bool EditingStyleUtilities::HasAncestorVerticalAlignStyle(Node& node,
                                                          CSSValueID value) {
  for (Node& runner : NodeTraversal::InclusiveAncestorsOf(node)) {
    CSSComputedStyleDeclaration* ancestor_style =
        CSSComputedStyleDeclaration::Create(&runner);
    if (GetIdentifierValue(ancestor_style, CSSPropertyVerticalAlign) == value)
      return true;
  }
  return false;
}

EditingStyle*
EditingStyleUtilities::CreateWrappingStyleForAnnotatedSerialization(
    ContainerNode* context) {
  // TODO(editing-dev): Change this function to take |const ContainerNode&|.
  // Tracking bug for this is crbug.com/766448.
  DCHECK(context);
  EditingStyle* wrapping_style =
      EditingStyle::Create(context, EditingStyle::kEditingPropertiesInEffect);

  // Styles that Mail blockquotes contribute should only be placed on the Mail
  // blockquote, to help us differentiate those styles from ones that the user
  // has applied. This helps us get the color of content pasted into
  // blockquotes right.
  wrapping_style->RemoveStyleAddedByElement(ToHTMLElement(EnclosingNodeOfType(
      FirstPositionInOrBeforeNode(*context), IsMailHTMLBlockquoteElement,
      kCanCrossEditingBoundary)));

  // Call collapseTextDecorationProperties first or otherwise it'll copy the
  // value over from in-effect to text-decorations.
  wrapping_style->CollapseTextDecorationProperties(
      context->GetDocument().GetSecureContextMode());

  return wrapping_style;
}

EditingStyle* EditingStyleUtilities::CreateWrappingStyleForSerialization(
    ContainerNode* context) {
  DCHECK(context);
  EditingStyle* wrapping_style = EditingStyle::Create();

  // When not annotating for interchange, we only preserve inline style
  // declarations.
  for (Node& node : NodeTraversal::InclusiveAncestorsOf(*context)) {
    if (node.IsDocumentNode())
      break;
    if (node.IsStyledElement() && !IsMailHTMLBlockquoteElement(&node)) {
      wrapping_style->MergeInlineAndImplicitStyleOfElement(
          ToElement(&node), EditingStyle::kDoNotOverrideValues,
          EditingStyle::kEditingPropertiesInEffect);
    }
  }

  return wrapping_style;
}

EditingStyle* EditingStyleUtilities::CreateStyleAtSelectionStart(
    const VisibleSelection& selection,
    bool should_use_background_color_in_effect,
    MutableCSSPropertyValueSet* style_to_check) {
  if (selection.IsNone())
    return nullptr;

  Document& document = *selection.Start().GetDocument();

  DCHECK(!document.NeedsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallow_transition(
      document.Lifecycle());

  // TODO(editing-dev): We should make |position| to |const Position&| by
  // integrating this expression and if-statement below.
  Position position =
      selection.IsCaret()
          ? CreateVisiblePosition(selection.Start()).DeepEquivalent()
          : AdjustedSelectionStartForStyleComputation(selection.Start());

  // If the pos is at the end of a text node, then this node is not fully
  // selected. Move it to the next deep equivalent position to avoid removing
  // the style from this node.
  // e.g. if pos was at Position("hello", 5) in <b>hello<div>world</div></b>, we
  // want Position("world", 0) instead.
  // We only do this for range because caret at Position("hello", 5) in
  // <b>hello</b>world should give you font-weight: bold.
  Node* position_node = position.ComputeContainerNode();
  if (selection.IsRange() && position_node && position_node->IsTextNode() &&
      position.ComputeOffsetInContainerNode() ==
          static_cast<int>(ToText(position_node)->length()))
    position = NextVisuallyDistinctCandidate(position);

  Element* element = AssociatedElementOf(position);
  if (!element)
    return nullptr;

  EditingStyle* style =
      EditingStyle::Create(element, EditingStyle::kAllProperties);
  style->MergeTypingStyle(&element->GetDocument());

  // If |element| has <sub> or <sup> ancestor element, apply the corresponding
  // style(vertical-align) to it so that document.queryCommandState() works with
  // the style. See bug http://crbug.com/582225.
  CSSValueID value_id =
      GetIdentifierValue(style_to_check, CSSPropertyVerticalAlign);
  if (value_id == CSSValueSub || value_id == CSSValueSuper) {
    CSSComputedStyleDeclaration* element_style =
        CSSComputedStyleDeclaration::Create(element);
    // Find the ancestor that has CSSValueSub or CSSValueSuper as the value of
    // CSS vertical-align property.
    if (GetIdentifierValue(element_style, CSSPropertyVerticalAlign) ==
            CSSValueBaseline &&
        HasAncestorVerticalAlignStyle(*element, value_id))
      style->Style()->SetProperty(CSSPropertyVerticalAlign, value_id);
  }

  // If background color is transparent, traverse parent nodes until we hit a
  // different value or document root Also, if the selection is a range, ignore
  // the background color at the start of selection, and find the background
  // color of the common ancestor.
  if (should_use_background_color_in_effect &&
      (selection.IsRange() || HasTransparentBackgroundColor(style->Style()))) {
    const EphemeralRange range(selection.ToNormalizedEphemeralRange());
    if (const CSSValue* value =
            BackgroundColorValueInEffect(range.CommonAncestorContainer())) {
      style->SetProperty(CSSPropertyBackgroundColor, value->CssText(),
                         /* important */ false,
                         document.GetSecureContextMode());
    }
  }

  return style;
}

bool EditingStyleUtilities::IsTransparentColorValue(const CSSValue* css_value) {
  if (!css_value)
    return true;
  if (css_value->IsColorValue())
    return !ToCSSColorValue(css_value)->Value().Alpha();
  if (!css_value->IsIdentifierValue())
    return false;
  return ToCSSIdentifierValue(css_value)->GetValueID() == CSSValueTransparent;
}

bool EditingStyleUtilities::HasTransparentBackgroundColor(
    CSSStyleDeclaration* style) {
  const CSSValue* css_value =
      style->GetPropertyCSSValueInternal(CSSPropertyBackgroundColor);
  return IsTransparentColorValue(css_value);
}

bool EditingStyleUtilities::HasTransparentBackgroundColor(
    CSSPropertyValueSet* style) {
  const CSSValue* css_value =
      style->GetPropertyCSSValue(CSSPropertyBackgroundColor);
  return IsTransparentColorValue(css_value);
}

const CSSValue* EditingStyleUtilities::BackgroundColorValueInEffect(
    Node* node) {
  for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
    CSSComputedStyleDeclaration* ancestor_style =
        CSSComputedStyleDeclaration::Create(ancestor);
    if (!HasTransparentBackgroundColor(ancestor_style)) {
      return ancestor_style->GetPropertyCSSValue(
          GetCSSPropertyBackgroundColor());
    }
  }
  return nullptr;
}

}  // namespace blink
