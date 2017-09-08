/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/resolver/StyleAdjuster.h"

#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/css/StyleChangeReason.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/css/resolver/StyleResolverState.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/HTMLTableCellElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/layout/LayoutObject.h"
#include "core/layout/LayoutTheme.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ComputedStyleConstants.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/Length.h"
#include "platform/transforms/TransformOperations.h"
#include "platform/wtf/Assertions.h"

namespace blink {

using namespace HTMLNames;

namespace {

TouchAction AdjustTouchActionForElement(TouchAction touch_action,
                                        const ComputedStyle& style,
                                        Element* element) {
  bool is_child_document =
      element && element == element->GetDocument().documentElement() &&
      element->GetDocument().LocalOwner();
  if (style.ScrollsOverflow() || is_child_document)
    return touch_action | TouchAction::kTouchActionPan;
  return touch_action;
}

}  // namespace

static EDisplay EquivalentBlockDisplay(EDisplay display) {
  switch (display) {
    case EDisplay::kBlock:
    case EDisplay::kTable:
    case EDisplay::kWebkitBox:
    case EDisplay::kFlex:
    case EDisplay::kGrid:
    case EDisplay::kListItem:
    case EDisplay::kFlowRoot:
      return display;
    case EDisplay::kInlineTable:
      return EDisplay::kTable;
    case EDisplay::kWebkitInlineBox:
      return EDisplay::kWebkitBox;
    case EDisplay::kInlineFlex:
      return EDisplay::kFlex;
    case EDisplay::kInlineGrid:
      return EDisplay::kGrid;

    case EDisplay::kContents:
    case EDisplay::kInline:
    case EDisplay::kInlineBlock:
    case EDisplay::kTableRowGroup:
    case EDisplay::kTableHeaderGroup:
    case EDisplay::kTableFooterGroup:
    case EDisplay::kTableRow:
    case EDisplay::kTableColumnGroup:
    case EDisplay::kTableColumn:
    case EDisplay::kTableCell:
    case EDisplay::kTableCaption:
      return EDisplay::kBlock;
    case EDisplay::kNone:
      NOTREACHED();
      return display;
  }
  NOTREACHED();
  return EDisplay::kBlock;
}

static bool IsOutermostSVGElement(const Element* element) {
  return element && element->IsSVGElement() &&
         ToSVGElement(*element).IsOutermostSVGSVGElement();
}

// CSS requires text-decoration to be reset at each DOM element for
// inline blocks, inline tables, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements. Outermost <svg> roots are
// considered to be atomic inline-level.
static bool DoesNotInheritTextDecoration(const ComputedStyle& style,
                                         const Element* element) {
  return style.Display() == EDisplay::kInlineTable ||
         style.Display() == EDisplay::kInlineBlock ||
         style.Display() == EDisplay::kWebkitInlineBox ||
         IsAtShadowBoundary(element) || style.IsFloating() ||
         style.HasOutOfFlowPosition() || IsOutermostSVGElement(element) ||
         isHTMLRTElement(element);
}

// Certain elements (<a>, <font>) override text decoration colors.  "The font
// element is expected to override the color of any text decoration that spans
// the text of the element to the used value of the element's 'color' property."
// (https://html.spec.whatwg.org/multipage/rendering.html#phrasing-content-3)
// The <a> behavior is non-standard.
static bool OverridesTextDecorationColors(const Element* element) {
  return element &&
         (isHTMLFontElement(element) || isHTMLAnchorElement(element));
}

// FIXME: This helper is only needed because pseudoStyleForElement passes a null
// element to adjustComputedStyle, so we can't just use element->isInTopLayer().
static bool IsInTopLayer(const Element* element, const ComputedStyle& style) {
  return (element && element->IsInTopLayer()) ||
         style.StyleType() == kPseudoIdBackdrop;
}

static bool LayoutParentStyleForcesZIndexToCreateStackingContext(
    const ComputedStyle& layout_parent_style) {
  return layout_parent_style.IsDisplayFlexibleOrGridBox();
}

void StyleAdjuster::AdjustStyleForEditing(ComputedStyle& style) {
  if (style.UserModify() != EUserModify::kReadWritePlaintextOnly)
    return;
  // Collapsing whitespace is harmful in plain-text editing.
  if (style.WhiteSpace() == EWhiteSpace::kNormal)
    style.SetWhiteSpace(EWhiteSpace::kPreWrap);
  else if (style.WhiteSpace() == EWhiteSpace::kNowrap)
    style.SetWhiteSpace(EWhiteSpace::kPre);
  else if (style.WhiteSpace() == EWhiteSpace::kPreLine)
    style.SetWhiteSpace(EWhiteSpace::kPreWrap);
}

static void AdjustStyleForFirstLetter(ComputedStyle& style) {
  if (style.StyleType() != kPseudoIdFirstLetter)
    return;

  // Force inline display (except for floating first-letters).
  style.SetDisplay(style.IsFloating() ? EDisplay::kBlock : EDisplay::kInline);

  // CSS2 says first-letter can't be positioned.
  style.SetPosition(EPosition::kStatic);
}

static void AdjustStyleForHTMLElement(ComputedStyle& style,
                                      HTMLElement& element) {
  // <div> and <span> are the most common elements on the web, we skip all the
  // work for them.
  if (isHTMLDivElement(element) || isHTMLSpanElement(element))
    return;

  if (IsHTMLTableCellElement(element)) {
    if (style.WhiteSpace() == EWhiteSpace::kWebkitNowrap) {
      // Figure out if we are really nowrapping or if we should just
      // use normal instead. If the width of the cell is fixed, then
      // we don't actually use NOWRAP.
      if (style.Width().IsFixed())
        style.SetWhiteSpace(EWhiteSpace::kNormal);
      else
        style.SetWhiteSpace(EWhiteSpace::kNowrap);
    }
    return;
  }

  if (isHTMLImageElement(element)) {
    if (toHTMLImageElement(element).IsCollapsed())
      style.SetDisplay(EDisplay::kNone);
    return;
  }

  if (isHTMLTableElement(element)) {
    // Tables never support the -webkit-* values for text-align and will reset
    // back to the default.
    if (style.GetTextAlign() == ETextAlign::kWebkitLeft ||
        style.GetTextAlign() == ETextAlign::kWebkitCenter ||
        style.GetTextAlign() == ETextAlign::kWebkitRight)
      style.SetTextAlign(ETextAlign::kStart);
    return;
  }

  if (isHTMLFrameElement(element) || isHTMLFrameSetElement(element)) {
    // Frames and framesets never honor position:relative or position:absolute.
    // This is necessary to fix a crash where a site tries to position these
    // objects. They also never honor display.
    style.SetPosition(EPosition::kStatic);
    style.SetDisplay(EDisplay::kBlock);
    return;
  }

  if (IsHTMLFrameElementBase(element)) {
    // Frames cannot overflow (they are always the size we ask them to be).
    // Some compositing code paths may try to draw scrollbars anyhow.
    style.SetOverflowX(EOverflow::kVisible);
    style.SetOverflowY(EOverflow::kVisible);
    return;
  }

  if (isHTMLRTElement(element)) {
    // Ruby text does not support float or position. This might change with
    // evolution of the specification.
    style.SetPosition(EPosition::kStatic);
    style.SetFloating(EFloat::kNone);
    return;
  }

  if (isHTMLLegendElement(element)) {
    style.SetDisplay(EDisplay::kBlock);
    return;
  }

  if (isHTMLMarqueeElement(element)) {
    // For now, <marquee> requires an overflow clip to work properly.
    style.SetOverflowX(EOverflow::kHidden);
    style.SetOverflowY(EOverflow::kHidden);
    return;
  }

  if (isHTMLTextAreaElement(element)) {
    // Textarea considers overflow visible as auto.
    style.SetOverflowX(style.OverflowX() == EOverflow::kVisible
                           ? EOverflow::kAuto
                           : style.OverflowX());
    style.SetOverflowY(style.OverflowY() == EOverflow::kVisible
                           ? EOverflow::kAuto
                           : style.OverflowY());
    return;
  }

  if (IsHTMLPlugInElement(element)) {
    style.SetRequiresAcceleratedCompositingForExternalReasons(
        ToHTMLPlugInElement(element).ShouldAccelerate());
    return;
  }
}

static void AdjustOverflow(ComputedStyle& style) {
  DCHECK(style.OverflowX() != EOverflow::kVisible ||
         style.OverflowY() != EOverflow::kVisible);

  if (style.Display() == EDisplay::kTable ||
      style.Display() == EDisplay::kInlineTable) {
    // Tables only support overflow:hidden and overflow:visible and ignore
    // anything else, see http://dev.w3.org/csswg/css2/visufx.html#overflow. As
    // a table is not a block container box the rules for resolving conflicting
    // x and y values in CSS Overflow Module Level 3 do not apply. Arguably
    // overflow-x and overflow-y aren't allowed on tables but all UAs allow it.
    if (style.OverflowX() != EOverflow::kHidden)
      style.SetOverflowX(EOverflow::kVisible);
    if (style.OverflowY() != EOverflow::kHidden)
      style.SetOverflowY(EOverflow::kVisible);
    // If we are left with conflicting overflow values for the x and y axes on a
    // table then resolve both to OverflowVisible. This is interoperable
    // behaviour but is not specced anywhere.
    if (style.OverflowX() == EOverflow::kVisible)
      style.SetOverflowY(EOverflow::kVisible);
    else if (style.OverflowY() == EOverflow::kVisible)
      style.SetOverflowX(EOverflow::kVisible);
  } else if (style.OverflowX() == EOverflow::kVisible &&
             style.OverflowY() != EOverflow::kVisible) {
    // If either overflow value is not visible, change to auto.
    // FIXME: Once we implement pagination controls, overflow-x should default
    // to hidden if overflow-y is set to -webkit-paged-x or -webkit-page-y. For
    // now, we'll let it default to auto so we can at least scroll through the
    // pages.
    style.SetOverflowX(EOverflow::kAuto);
  } else if (style.OverflowY() == EOverflow::kVisible &&
             style.OverflowX() != EOverflow::kVisible) {
    style.SetOverflowY(EOverflow::kAuto);
  }

  // Menulists should have visible overflow
  if (style.Appearance() == kMenulistPart) {
    style.SetOverflowX(EOverflow::kVisible);
    style.SetOverflowY(EOverflow::kVisible);
  }
}

static void AdjustStyleForDisplay(ComputedStyle& style,
                                  const ComputedStyle& layout_parent_style,
                                  Document* document) {
  if (style.Display() == EDisplay::kBlock && !style.IsFloating())
    return;

  if (style.Display() == EDisplay::kContents)
    return;

  // FIXME: Don't support this mutation for pseudo styles like first-letter or
  // first-line, since it's not completely clear how that should work.
  if (style.Display() == EDisplay::kInline &&
      style.StyleType() == kPseudoIdNone &&
      style.GetWritingMode() != layout_parent_style.GetWritingMode())
    style.SetDisplay(EDisplay::kInlineBlock);

  // We do not honor position: relative or sticky for table rows, headers, and
  // footers. This is correct for position: relative in CSS2.1 (and caused a
  // crash in containingBlock() on some sites) and position: sticky is defined
  // as following position: relative behavior for table elements. It is
  // incorrect for CSS3.
  if ((style.Display() == EDisplay::kTableHeaderGroup ||
       style.Display() == EDisplay::kTableRowGroup ||
       style.Display() == EDisplay::kTableFooterGroup ||
       style.Display() == EDisplay::kTableRow) &&
      style.HasInFlowPosition())
    style.SetPosition(EPosition::kStatic);

  // Cannot support position: sticky for table columns and column groups because
  // current code is only doing background painting through columns / column
  // groups.
  if ((style.Display() == EDisplay::kTableColumnGroup ||
       style.Display() == EDisplay::kTableColumn) &&
      style.GetPosition() == EPosition::kSticky)
    style.SetPosition(EPosition::kStatic);

  // writing-mode does not apply to table row groups, table column groups, table
  // rows, and table columns.
  // FIXME: Table cells should be allowed to be perpendicular or flipped with
  // respect to the table, though.
  if (style.Display() == EDisplay::kTableColumn ||
      style.Display() == EDisplay::kTableColumnGroup ||
      style.Display() == EDisplay::kTableFooterGroup ||
      style.Display() == EDisplay::kTableHeaderGroup ||
      style.Display() == EDisplay::kTableRow ||
      style.Display() == EDisplay::kTableRowGroup ||
      style.Display() == EDisplay::kTableCell)
    style.SetWritingMode(layout_parent_style.GetWritingMode());

  // FIXME: Since we don't support block-flow on flexible boxes yet, disallow
  // setting of block-flow to anything other than TopToBottomWritingMode.
  // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
  if (style.GetWritingMode() != WritingMode::kHorizontalTb &&
      (style.Display() == EDisplay::kWebkitBox ||
       style.Display() == EDisplay::kWebkitInlineBox))
    style.SetWritingMode(WritingMode::kHorizontalTb);

  if (layout_parent_style.IsDisplayFlexibleOrGridBox()) {
    style.SetFloating(EFloat::kNone);
    style.SetDisplay(EquivalentBlockDisplay(style.Display()));

    // We want to count vertical percentage paddings/margins on flex items
    // because our current behavior is different from the spec and we want to
    // gather compatibility data.
    if (style.PaddingBefore().IsPercentOrCalc() ||
        style.PaddingAfter().IsPercentOrCalc()) {
      UseCounter::Count(document,
                        WebFeature::kFlexboxPercentagePaddingVertical);
    }
    if (style.MarginBefore().IsPercentOrCalc() ||
        style.MarginAfter().IsPercentOrCalc()) {
      UseCounter::Count(document, WebFeature::kFlexboxPercentageMarginVertical);
    }
  }
}

static void AdjustEffectiveTouchAction(ComputedStyle& style,
                                       const ComputedStyle& parent_style,
                                       Element* element) {
  TouchAction inherited_action = parent_style.GetEffectiveTouchAction();

  bool is_svg_root = element && element->IsSVGElement() &&
                     isSVGSVGElement(*element) && element->parentNode() &&
                     !element->parentNode()->IsSVGElement();
  bool is_replaced_canvas =
      element && isHTMLCanvasElement(element) &&
      element->GetDocument().GetFrame() &&
      element->GetDocument().CanExecuteScripts(kNotAboutToExecuteScript);
  bool is_non_replaced_inline_elements =
      style.IsDisplayInlineType() &&
      !(style.IsDisplayReplacedType() || is_svg_root ||
        isHTMLImageElement(element) || is_replaced_canvas);
  bool is_table_row_or_column = style.IsDisplayTableRowOrColumnType();
  bool is_layout_object_needed =
      element && element->LayoutObjectIsNeeded(style);

  TouchAction element_touch_action = TouchAction::kTouchActionAuto;
  // Touch actions are only supported by elements that support both the CSS
  // width and height properties.
  // See https://www.w3.org/TR/pointerevents/#the-touch-action-css-property.
  if (!is_non_replaced_inline_elements && !is_table_row_or_column &&
      is_layout_object_needed) {
    element_touch_action = style.GetTouchAction();
  }

  bool is_child_document =
      element && element == element->GetDocument().documentElement() &&
      element->GetDocument().LocalOwner();

  if (is_child_document) {
    const ComputedStyle* frame_style =
        element->GetDocument().LocalOwner()->GetComputedStyle();
    if (frame_style)
      inherited_action = frame_style->GetEffectiveTouchAction();
  }

  // The effective touch action is the intersection of the touch-action values
  // of the current element and all of its ancestors up to the one that
  // implements the gesture. Since panning is implemented by the scroller it is
  // re-enabled for scrolling elements.
  // The panning-restricted cancellation should also apply to iframes, so we
  // allow (panning & local touch action) on the first descendant element of a
  // iframe element.
  inherited_action =
      AdjustTouchActionForElement(inherited_action, style, element);

  // Apply the adjusted parent effective touch actions.
  style.SetEffectiveTouchAction(element_touch_action & inherited_action);

  // Touch action is inherited across frames.
  if (element && element->IsFrameOwnerElement() &&
      ToHTMLFrameOwnerElement(element)->contentDocument()) {
    Element* content_document_element =
        ToHTMLFrameOwnerElement(element)->contentDocument()->documentElement();
    if (content_document_element) {
      // Actively trigger recalc for child document if the document does not
      // have computed style created, or its effective touch action is out of
      // date.
      bool child_document_needs_recalc = true;
      if (const ComputedStyle* content_document_style =
              content_document_element->GetComputedStyle()) {
        TouchAction document_touch_action =
            content_document_style->GetEffectiveTouchAction();
        TouchAction expected_document_touch_action =
            AdjustTouchActionForElement(style.GetEffectiveTouchAction(),
                                        *content_document_style,
                                        content_document_element) &
            content_document_style->GetTouchAction();
        child_document_needs_recalc =
            document_touch_action != expected_document_touch_action;
      }
      if (child_document_needs_recalc) {
        content_document_element->SetNeedsStyleRecalc(
            kSubtreeStyleChange,
            StyleChangeReasonForTracing::Create(
                StyleChangeReason::kInheritedStyleChangeFromParentFrame));
      }
    }
  }
}

void StyleAdjuster::AdjustComputedStyle(StyleResolverState& state,
                                        Element* element) {
  DCHECK(state.LayoutParentStyle());
  DCHECK(state.ParentStyle());
  ComputedStyle& style = state.MutableStyleRef();
  const ComputedStyle& parent_style = *state.ParentStyle();
  const ComputedStyle& layout_parent_style = *state.LayoutParentStyle();

  if (style.Display() != EDisplay::kNone) {
    if (element && element->IsHTMLElement())
      AdjustStyleForHTMLElement(style, ToHTMLElement(*element));

    // Per the spec, position 'static' and 'relative' in the top layer compute
    // to 'absolute'.
    if (IsInTopLayer(element, style) &&
        (style.GetPosition() == EPosition::kStatic ||
         style.GetPosition() == EPosition::kRelative))
      style.SetPosition(EPosition::kAbsolute);

    // Absolute/fixed positioned elements, floating elements and the document
    // element need block-like outside display.
    if (style.Display() != EDisplay::kContents &&
        (style.HasOutOfFlowPosition() || style.IsFloating()))
      style.SetDisplay(EquivalentBlockDisplay(style.Display()));

    if (element && element->GetDocument().documentElement() == element)
      style.SetDisplay(EquivalentBlockDisplay(style.Display()));

    // We don't adjust the first letter style earlier because we may change the
    // display setting in adjustStyeForTagName() above.
    AdjustStyleForFirstLetter(style);

    AdjustStyleForDisplay(style, layout_parent_style,
                          element ? &element->GetDocument() : 0);

    // Paint containment forces a block formatting context, so we must coerce
    // from inline.  https://drafts.csswg.org/css-containment/#containment-paint
    if (style.ContainsPaint() && style.Display() == EDisplay::kInline)
      style.SetDisplay(EDisplay::kBlock);
  } else {
    AdjustStyleForFirstLetter(style);
  }

  // Make sure our z-index value is only applied if the object is positioned.
  if (style.GetPosition() == EPosition::kStatic &&
      !LayoutParentStyleForcesZIndexToCreateStackingContext(
          layout_parent_style)) {
    style.SetIsStackingContext(false);
    // TODO(alancutter): Avoid altering z-index here.
    if (!style.HasAutoZIndex())
      style.SetZIndex(0);
  } else if (!style.HasAutoZIndex()) {
    style.SetIsStackingContext(true);
  }

  if (style.OverflowX() != EOverflow::kVisible ||
      style.OverflowY() != EOverflow::kVisible)
    AdjustOverflow(style);

  if (DoesNotInheritTextDecoration(style, element))
    style.ClearAppliedTextDecorations();
  else
    style.RestoreParentTextDecorations(parent_style);
  style.ApplyTextDecorations(
      parent_style.VisitedDependentColor(CSSPropertyTextDecorationColor),
      OverridesTextDecorationColors(element));

  // Cull out any useless layers and also repeat patterns into additional
  // layers.
  style.AdjustBackgroundLayers();
  style.AdjustMaskLayers();

  // Let the theme also have a crack at adjusting the style.
  if (style.HasAppearance())
    LayoutTheme::GetTheme().AdjustStyle(style, element);

  // If we have first-letter pseudo style, transitions, or animations, do not
  // share this style.
  if (style.HasPseudoStyle(kPseudoIdFirstLetter) || style.Transitions() ||
      style.Animations())
    style.SetUnique();

  AdjustStyleForEditing(style);

  bool is_svg_element = element && element->IsSVGElement();
  if (is_svg_element) {
    // display: contents computes to inline for replaced elements and form
    // controls, and isn't specified for other kinds of SVG content[1], so let's
    // just do the same here for all other SVG elements.
    //
    // If we wouldn't do this, then we'd need to ensure that display: contents
    // doesn't prevent SVG elements from generating a LayoutObject in
    // SVGElement::LayoutObjectIsNeeded.
    //
    // [1]: https://www.w3.org/TR/SVG/painting.html#DisplayProperty
    if (style.Display() == EDisplay::kContents)
      style.SetDisplay(EDisplay::kInline);

    // Only the root <svg> element in an SVG document fragment tree honors css
    // position.
    if (!(isSVGSVGElement(*element) && element->parentNode() &&
          !element->parentNode()->IsSVGElement()))
      style.SetPosition(ComputedStyle::InitialPosition());

    // SVG text layout code expects us to be a block-level style element.
    if ((isSVGForeignObjectElement(*element) || isSVGTextElement(*element)) &&
        style.IsDisplayInlineType())
      style.SetDisplay(EDisplay::kBlock);

    // Columns don't apply to svg text elements.
    if (isSVGTextElement(*element))
      style.ClearMultiCol();
  }

  // If this node is sticky it marks the creation of a sticky subtree, which we
  // must track to properly handle document lifecycle in some cases.
  //
  // It is possible that this node is already in a sticky subtree (i.e. we have
  // nested sticky nodes) - in that case the bit will already be set via
  // inheritance from the ancestor and there is no harm to setting it again.
  if (style.GetPosition() == EPosition::kSticky)
    style.SetSubtreeIsSticky(true);

  // If the inherited value of justify-items includes the 'legacy' keyword,
  // 'auto' computes to the the inherited value.  Otherwise, 'auto' computes to
  // 'normal'.
  if (style.JustifyItemsPosition() == kItemPositionAuto) {
    if (parent_style.JustifyItemsPositionType() == kLegacyPosition)
      style.SetJustifyItems(parent_style.JustifyItems());
  }

  AdjustEffectiveTouchAction(style, parent_style, element);

  bool is_media_control =
      element && element->ShadowPseudoId().StartsWith("-webkit-media-controls");
  if (is_media_control && style.Appearance() == kNoControlPart) {
    // For compatibility reasons if the element is a media control and the
    // -webkit-appearance is none then we should clear the background image.
    if (!StyleResolver::HasAuthorBackground(state)) {
      style.MutableBackgroundInternal().ClearImage();
    }
  }
}
}  // namespace blink
