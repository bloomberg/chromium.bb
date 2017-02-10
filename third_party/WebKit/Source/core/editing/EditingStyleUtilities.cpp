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

#include "EditingStyleUtilities.h"

#include "core/css/CSSColorValue.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/editing/EditingStyle.h"
#include "core/editing/EditingUtilities.h"

namespace blink {

bool EditingStyleUtilities::hasAncestorVerticalAlignStyle(Node& node,
                                                          CSSValueID value) {
  for (Node& runner : NodeTraversal::inclusiveAncestorsOf(node)) {
    CSSComputedStyleDeclaration* ancestorStyle =
        CSSComputedStyleDeclaration::create(&runner);
    if (getIdentifierValue(ancestorStyle, CSSPropertyVerticalAlign) == value)
      return true;
  }
  return false;
}

EditingStyle*
EditingStyleUtilities::createWrappingStyleForAnnotatedSerialization(
    ContainerNode* context) {
  EditingStyle* wrappingStyle =
      EditingStyle::create(context, EditingStyle::EditingPropertiesInEffect);

  // Styles that Mail blockquotes contribute should only be placed on the Mail
  // blockquote, to help us differentiate those styles from ones that the user
  // has applied. This helps us get the color of content pasted into
  // blockquotes right.
  wrappingStyle->removeStyleAddedByElement(toHTMLElement(enclosingNodeOfType(
      firstPositionInOrBeforeNode(context), isMailHTMLBlockquoteElement,
      CanCrossEditingBoundary)));

  // Call collapseTextDecorationProperties first or otherwise it'll copy the
  // value over from in-effect to text-decorations.
  wrappingStyle->collapseTextDecorationProperties();

  return wrappingStyle;
}

EditingStyle* EditingStyleUtilities::createWrappingStyleForSerialization(
    ContainerNode* context) {
  DCHECK(context);
  EditingStyle* wrappingStyle = EditingStyle::create();

  // When not annotating for interchange, we only preserve inline style
  // declarations.
  for (Node& node : NodeTraversal::inclusiveAncestorsOf(*context)) {
    if (node.isDocumentNode())
      break;
    if (node.isStyledElement() && !isMailHTMLBlockquoteElement(&node)) {
      wrappingStyle->mergeInlineAndImplicitStyleOfElement(
          toElement(&node), EditingStyle::DoNotOverrideValues,
          EditingStyle::EditingPropertiesInEffect);
    }
  }

  return wrappingStyle;
}

EditingStyle* EditingStyleUtilities::createStyleAtSelectionStart(
    const VisibleSelection& selection,
    bool shouldUseBackgroundColorInEffect,
    MutableStylePropertySet* styleToCheck) {
  if (selection.isNone())
    return nullptr;

  Document& document = *selection.start().document();

  DCHECK(!document.needsLayoutTreeUpdate());
  DocumentLifecycle::DisallowTransitionScope disallowTransition(
      document.lifecycle());

  Position position = adjustedSelectionStartForStyleComputation(selection);

  // If the pos is at the end of a text node, then this node is not fully
  // selected. Move it to the next deep equivalent position to avoid removing
  // the style from this node.
  // e.g. if pos was at Position("hello", 5) in <b>hello<div>world</div></b>, we
  // want Position("world", 0) instead.
  // We only do this for range because caret at Position("hello", 5) in
  // <b>hello</b>world should give you font-weight: bold.
  Node* positionNode = position.computeContainerNode();
  if (selection.isRange() && positionNode && positionNode->isTextNode() &&
      position.computeOffsetInContainerNode() ==
          positionNode->maxCharacterOffset())
    position = nextVisuallyDistinctCandidate(position);

  Element* element = associatedElementOf(position);
  if (!element)
    return nullptr;

  EditingStyle* style =
      EditingStyle::create(element, EditingStyle::AllProperties);
  style->mergeTypingStyle(&element->document());

  // If |element| has <sub> or <sup> ancestor element, apply the corresponding
  // style(vertical-align) to it so that document.queryCommandState() works with
  // the style. See bug http://crbug.com/582225.
  CSSValueID valueID =
      getIdentifierValue(styleToCheck, CSSPropertyVerticalAlign);
  if (valueID == CSSValueSub || valueID == CSSValueSuper) {
    CSSComputedStyleDeclaration* elementStyle =
        CSSComputedStyleDeclaration::create(element);
    // Find the ancestor that has CSSValueSub or CSSValueSuper as the value of
    // CSS vertical-align property.
    if (getIdentifierValue(elementStyle, CSSPropertyVerticalAlign) ==
            CSSValueBaseline &&
        hasAncestorVerticalAlignStyle(*element, valueID))
      style->style()->setProperty(CSSPropertyVerticalAlign, valueID);
  }

  // If background color is transparent, traverse parent nodes until we hit a
  // different value or document root Also, if the selection is a range, ignore
  // the background color at the start of selection, and find the background
  // color of the common ancestor.
  if (shouldUseBackgroundColorInEffect &&
      (selection.isRange() || hasTransparentBackgroundColor(style->style()))) {
    const EphemeralRange range(selection.toNormalizedEphemeralRange());
    if (const CSSValue* value =
            backgroundColorValueInEffect(Range::commonAncestorContainer(
                range.startPosition().computeContainerNode(),
                range.endPosition().computeContainerNode())))
      style->setProperty(CSSPropertyBackgroundColor, value->cssText());
  }

  return style;
}

static bool isUnicodeBidiNestedOrMultipleEmbeddings(CSSValueID valueID) {
  return valueID == CSSValueEmbed || valueID == CSSValueBidiOverride ||
         valueID == CSSValueWebkitIsolate ||
         valueID == CSSValueWebkitIsolateOverride ||
         valueID == CSSValueWebkitPlaintext || valueID == CSSValueIsolate ||
         valueID == CSSValueIsolateOverride || valueID == CSSValuePlaintext;
}

WritingDirection EditingStyleUtilities::textDirectionForSelection(
    const VisibleSelection& selection,
    EditingStyle* typingStyle,
    bool& hasNestedOrMultipleEmbeddings) {
  hasNestedOrMultipleEmbeddings = true;

  if (selection.isNone())
    return NaturalWritingDirection;

  Position position = mostForwardCaretPosition(selection.start());

  Node* node = position.anchorNode();
  if (!node)
    return NaturalWritingDirection;

  Position end;
  if (selection.isRange()) {
    end = mostBackwardCaretPosition(selection.end());

    DCHECK(end.document());
    const EphemeralRange caretRange(position.parentAnchoredEquivalent(),
                                    end.parentAnchoredEquivalent());
    for (Node& n : caretRange.nodes()) {
      if (!n.isStyledElement())
        continue;

      CSSComputedStyleDeclaration* style =
          CSSComputedStyleDeclaration::create(&n);
      const CSSValue* unicodeBidi =
          style->getPropertyCSSValue(CSSPropertyUnicodeBidi);
      if (!unicodeBidi || !unicodeBidi->isIdentifierValue())
        continue;

      CSSValueID unicodeBidiValue =
          toCSSIdentifierValue(unicodeBidi)->getValueID();
      if (isUnicodeBidiNestedOrMultipleEmbeddings(unicodeBidiValue))
        return NaturalWritingDirection;
    }
  }

  if (selection.isCaret()) {
    WritingDirection direction;
    if (typingStyle && typingStyle->textDirection(direction)) {
      hasNestedOrMultipleEmbeddings = false;
      return direction;
    }
    node = selection.visibleStart().deepEquivalent().anchorNode();
  }
  DCHECK(node);

  // The selection is either a caret with no typing attributes or a range in
  // which no embedding is added, so just use the start position to decide.
  Node* block = enclosingBlock(node);
  WritingDirection foundDirection = NaturalWritingDirection;

  for (Node& runner : NodeTraversal::inclusiveAncestorsOf(*node)) {
    if (runner == block)
      break;
    if (!runner.isStyledElement())
      continue;

    Element* element = &toElement(runner);
    CSSComputedStyleDeclaration* style =
        CSSComputedStyleDeclaration::create(element);
    const CSSValue* unicodeBidi =
        style->getPropertyCSSValue(CSSPropertyUnicodeBidi);
    if (!unicodeBidi || !unicodeBidi->isIdentifierValue())
      continue;

    CSSValueID unicodeBidiValue =
        toCSSIdentifierValue(unicodeBidi)->getValueID();
    if (unicodeBidiValue == CSSValueNormal)
      continue;

    if (unicodeBidiValue == CSSValueBidiOverride)
      return NaturalWritingDirection;

    DCHECK(isEmbedOrIsolate(unicodeBidiValue)) << unicodeBidiValue;
    const CSSValue* direction =
        style->getPropertyCSSValue(CSSPropertyDirection);
    if (!direction || !direction->isIdentifierValue())
      continue;

    int directionValue = toCSSIdentifierValue(direction)->getValueID();
    if (directionValue != CSSValueLtr && directionValue != CSSValueRtl)
      continue;

    if (foundDirection != NaturalWritingDirection)
      return NaturalWritingDirection;

    // In the range case, make sure that the embedding element persists until
    // the end of the range.
    if (selection.isRange() && !end.anchorNode()->isDescendantOf(element))
      return NaturalWritingDirection;

    foundDirection = directionValue == CSSValueLtr
                         ? LeftToRightWritingDirection
                         : RightToLeftWritingDirection;
  }
  hasNestedOrMultipleEmbeddings = false;
  return foundDirection;
}

bool EditingStyleUtilities::isTransparentColorValue(const CSSValue* cssValue) {
  if (!cssValue)
    return true;
  if (cssValue->isColorValue())
    return !toCSSColorValue(cssValue)->value().alpha();
  if (!cssValue->isIdentifierValue())
    return false;
  return toCSSIdentifierValue(cssValue)->getValueID() == CSSValueTransparent;
}

bool EditingStyleUtilities::hasTransparentBackgroundColor(
    CSSStyleDeclaration* style) {
  const CSSValue* cssValue =
      style->getPropertyCSSValueInternal(CSSPropertyBackgroundColor);
  return isTransparentColorValue(cssValue);
}

bool EditingStyleUtilities::hasTransparentBackgroundColor(
    StylePropertySet* style) {
  const CSSValue* cssValue =
      style->getPropertyCSSValue(CSSPropertyBackgroundColor);
  return isTransparentColorValue(cssValue);
}

const CSSValue* EditingStyleUtilities::backgroundColorValueInEffect(
    Node* node) {
  for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
    CSSComputedStyleDeclaration* ancestorStyle =
        CSSComputedStyleDeclaration::create(ancestor);
    if (!hasTransparentBackgroundColor(ancestorStyle))
      return ancestorStyle->getPropertyCSSValue(CSSPropertyBackgroundColor);
  }
  return nullptr;
}

}  // namespace blink
