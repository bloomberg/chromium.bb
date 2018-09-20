// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

enum AccessibilityOrientation {
  kAccessibilityOrientationUndefined = 0,
  kAccessibilityOrientationVertical,
  kAccessibilityOrientationHorizontal,
};

enum class AXDefaultActionVerb {
  kNone = 0,
  kActivate,
  kCheck,
  kClick,

  // A click will be performed on one of the object's ancestors.
  // This happens when the object itself is not clickable, but one of its
  // ancestors has click handlers attached which are able to capture the click
  // as it bubbles up.
  kClickAncestor,

  kJump,
  kOpen,
  kPress,
  kSelect,
  kUncheck
};

// The input restriction on an object.
enum AXRestriction {
  kNone = 0,  // An object that is not disabled.
  kReadOnly,
  kDisabled,
};

enum class AXSupportedAction {
  kNone = 0,
  kActivate,
  kCheck,
  kClick,
  kJump,
  kOpen,
  kPress,
  kSelect,
  kUncheck
};

enum AccessibilityTextDirection {
  kAccessibilityTextDirectionLTR,
  kAccessibilityTextDirectionRTL,
  kAccessibilityTextDirectionTTB,
  kAccessibilityTextDirectionBTT
};

enum AXTextPosition {
  kAXTextPositionNone = 0,
  kAXTextPositionSubscript,
  kAXTextPositionSuperscript
};

enum SortDirection {
  kSortDirectionUndefined = 0,
  kSortDirectionNone,
  kSortDirectionAscending,
  kSortDirectionDescending,
  kSortDirectionOther
};

enum AccessibilityExpanded {
  kExpandedUndefined = 0,
  kExpandedCollapsed,
  kExpandedExpanded,
};

enum AccessibilitySelectedState {
  kSelectedStateUndefined = 0,
  kSelectedStateFalse,
  kSelectedStateTrue,
};

enum AriaCurrentState {
  kAriaCurrentStateUndefined = 0,
  kAriaCurrentStateFalse,
  kAriaCurrentStateTrue,
  kAriaCurrentStatePage,
  kAriaCurrentStateStep,
  kAriaCurrentStateLocation,
  kAriaCurrentStateDate,
  kAriaCurrentStateTime
};

enum AXHasPopup {
  kAXHasPopupFalse = 0,
  kAXHasPopupTrue,
  kAXHasPopupMenu,
  kAXHasPopupListbox,
  kAXHasPopupTree,
  kAXHasPopupGrid,
  kAXHasPopupDialog
};

enum InvalidState {
  kInvalidStateUndefined = 0,
  kInvalidStateFalse,
  kInvalidStateTrue,
  kInvalidStateSpelling,
  kInvalidStateGrammar,
  kInvalidStateOther
};

enum TextStyle {
  kTextStyleNone = 0,
  kTextStyleBold = 1 << 0,
  kTextStyleItalic = 1 << 1,
  kTextStyleUnderline = 1 << 2,
  kTextStyleLineThrough = 1 << 3
};

enum class AXBoolAttribute {
  kAriaBusy,
};

enum class AXStringAttribute {
  kAriaKeyShortcuts,
  kAriaRoleDescription,
};

enum class AXObjectAttribute {
  kAriaActiveDescendant,
  kAriaDetails,
  kAriaErrorMessage,
};

enum class AXObjectVectorAttribute {
  kAriaControls,
  kAriaFlowTo,
};

// The source of the accessible name of an element. This is needed
// because on some platforms this determines how the accessible name
// is exposed.
enum AXNameFrom {
  kAXNameFromUninitialized = -1,
  kAXNameFromAttribute = 0,
  kAXNameFromAttributeExplicitlyEmpty,
  kAXNameFromCaption,
  kAXNameFromContents,
  kAXNameFromPlaceholder,
  kAXNameFromRelatedElement,
  kAXNameFromValue,
  kAXNameFromTitle,
};

// The source of the accessible description of an element. This is needed
// because on some platforms this determines how the accessible description
// is exposed.
enum AXDescriptionFrom {
  kAXDescriptionFromUninitialized = -1,
  kAXDescriptionFromAttribute = 0,
  kAXDescriptionFromContents,
  kAXDescriptionFromRelatedElement,
};

enum AXObjectInclusion {
  kIncludeObject,
  kIgnoreObject,
  kDefaultBehavior,
};

enum AccessibilityCheckedState {
  kCheckedStateUndefined = 0,
  kCheckedStateFalse,
  kCheckedStateTrue,
  kCheckedStateMixed
};

enum AccessibilityOptionalBool {
  kOptionalBoolUndefined = 0,
  kOptionalBoolTrue,
  kOptionalBoolFalse
};

// The potential native HTML-based text (name, description or placeholder)
// sources for an element.  See
// http://rawgit.com/w3c/aria/master/html-aam/html-aam.html#accessible-name-and-description-calculation
enum AXTextFromNativeHTML {
  kAXTextFromNativeHTMLUninitialized = -1,
  kAXTextFromNativeHTMLFigcaption,
  kAXTextFromNativeHTMLLabel,
  kAXTextFromNativeHTMLLabelFor,
  kAXTextFromNativeHTMLLabelWrapped,
  kAXTextFromNativeHTMLLegend,
  kAXTextFromNativeHTMLTableCaption,
  kAXTextFromNativeHTMLTitleElement,
};

enum AXIgnoredReason {
  kAXActiveModalDialog,
  kAXAncestorIsLeafNode,
  kAXAriaHiddenElement,
  kAXAriaHiddenSubtree,
  kAXEmptyAlt,
  kAXEmptyText,
  kAXInertElement,
  kAXInertSubtree,
  kAXInheritsPresentation,
  kAXLabelContainer,
  kAXLabelFor,
  kAXNotRendered,
  kAXNotVisible,
  kAXPresentational,
  kAXProbablyPresentational,
  kAXStaticTextUsedAsNameFor,
  kAXUninteresting
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ACCESSIBILITY_AX_ENUMS_H_
