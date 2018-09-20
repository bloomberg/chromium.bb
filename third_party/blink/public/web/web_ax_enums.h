/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_ENUMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_AX_ENUMS_H_

namespace blink {

enum class WebAXDefaultActionVerb {
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

enum WebAXTextDirection {
  kWebAXTextDirectionLR,
  kWebAXTextDirectionRL,
  kWebAXTextDirectionTB,
  kWebAXTextDirectionBT
};

enum WebAXTextPosition {
  kWebAXTextPositionNone = 0,
  kWebAXTextPositionSubscript,
  kWebAXTextPositionSuperscript
};

// Sort direction, only used for roles = ax::mojom::RoleRowHeader and
// ax::mojom::RoleColumnHeader.
enum WebAXSortDirection {
  kWebAXSortDirectionUndefined = 0,
  kWebAXSortDirectionNone,
  kWebAXSortDirectionAscending,
  kWebAXSortDirectionDescending,
  kWebAXSortDirectionOther
};

enum WebAXCheckedState {
  kWebAXCheckedUndefined = 0,
  kWebAXCheckedFalse,
  kWebAXCheckedTrue,
  kWebAXCheckedMixed
};

// Expanded State.
// These values must match blink::AccessibilityExpanded values.
// Enforced in AssertMatchingEnums.cpp.
enum WebAXExpanded {
  kWebAXExpandedUndefined = 0,
  kWebAXExpandedCollapsed,
  kWebAXExpandedExpanded
};

// Selected State.
// These values must match blink::AccessibilitySelectedState values.
enum WebAXSelectedState {
  kWebAXSelectedStateUndefined = 0,
  kWebAXSelectedStateFalse,
  kWebAXSelectedStateTrue
};

// These values must match blink::AccessibilityOrientation values.
// Enforced in AssertMatchingEnums.cpp.
enum WebAXOrientation {
  kWebAXOrientationUndefined = 0,
  kWebAXOrientationVertical,
  kWebAXOrientationHorizontal,
};

enum WebAXAriaCurrentState {
  kWebAXAriaCurrentStateUndefined = 0,
  kWebAXAriaCurrentStateFalse,
  kWebAXAriaCurrentStateTrue,
  kWebAXAriaCurrentStatePage,
  kWebAXAriaCurrentStateStep,
  kWebAXAriaCurrentStateLocation,
  kWebAXAriaCurrentStateDate,
  kWebAXAriaCurrentStateTime
};

enum WebAXHasPopup {
  kWebAXHasPopupFalse = 0,
  kWebAXHasPopupTrue,
  kWebAXHasPopupMenu,
  kWebAXHasPopupListbox,
  kWebAXHasPopupTree,
  kWebAXHasPopupGrid,
  kWebAXHasPopupDialog
};

// Only used by HTML form controls and any other element that has
// an aria-invalid attribute specified.
enum WebAXInvalidState {
  kWebAXInvalidStateUndefined = 0,
  kWebAXInvalidStateFalse,
  kWebAXInvalidStateTrue,
  kWebAXInvalidStateSpelling,
  kWebAXInvalidStateGrammar,
  kWebAXInvalidStateOther
};

// State of a form control or editors
enum WebAXRestriction {
  kWebAXRestrictionNone = 0,  // Enabled control or other object not disabled
  kWebAXRestrictionReadOnly,
  kWebAXRestrictionDisabled,
};

enum WebAXMarkerType {
  kWebAXMarkerTypeSpelling = 1 << 0,
  kWebAXMarkerTypeGrammar = 1 << 1,
  kWebAXMarkerTypeTextMatch = 1 << 2,
  // Skip DocumentMarker::MarkerType::Composition
  kWebAXMarkerTypeActiveSuggestion = 1 << 4,
  kWebAXMarkerTypeSuggestion = 1 << 5,
};

// Used for exposing text attributes.
enum WebAXTextStyle {
  kWebAXTextStyleNone = 0,
  kWebAXTextStyleBold = 1 << 0,
  kWebAXTextStyleItalic = 1 << 1,
  kWebAXTextStyleUnderline = 1 << 2,
  kWebAXTextStyleLineThrough = 1 << 3
};

// The source of the accessible name of an element. This is needed
// because on some platforms this determines how the accessible name
// is exposed.
enum WebAXNameFrom {
  kWebAXNameFromUninitialized = -1,
  kWebAXNameFromAttribute = 0,
  kWebAXNameFromAttributeExplicitlyEmpty,
  kWebAXNameFromCaption,
  kWebAXNameFromContents,
  kWebAXNameFromPlaceholder,
  kWebAXNameFromRelatedElement,
  kWebAXNameFromValue,
  kWebAXNameFromTitle,
};

// The source of the accessible description of an element. This is needed
// because on some platforms this determines how the accessible description
// is exposed.
enum WebAXDescriptionFrom {
  kWebAXDescriptionFromUninitialized = -1,
  kWebAXDescriptionFromAttribute = 0,
  kWebAXDescriptionFromContents,
  kWebAXDescriptionFromRelatedElement,
};

// Text affinity for the start or end of a selection.
enum WebAXTextAffinity {
  kWebAXTextAffinityUpstream,
  kWebAXTextAffinityDownstream
};

//
// Sparse accessibility attributes
//
// The following enums represent accessibility attributes that apply
// to only a small fraction of WebAXObjects. Rather than the client
// asking each WebAXObject for the value of each accessibility
// attribute, it can call a single function to query for all
// sparse attributes at the same time. Any sparse attributes that
// are present are returned via a callback consisting of an attribute
// key enum and an attribute value.
//

// Sparse attributes of a WebAXObject whose value is either true or
// false. In order for it to be a sparse attribute the default value
// must be false.
enum class WebAXBoolAttribute {
  kAriaBusy,
};

// Sparse attributes of a WebAXObject whose value is a string.
// In order for it to be a sparse attribute the default value
// must be "".
enum class WebAXStringAttribute {
  kAriaKeyShortcuts,
  kAriaRoleDescription,
};

// Sparse attributes of a WebAXObject whose value is a reference to
// another WebAXObject within the same frame. In order for it to be a
// sparse attribute the default value must be the null WebAXObject.
enum class WebAXObjectAttribute {
  kAriaActiveDescendant,
  kAriaDetails,
  kAriaErrorMessage,
};

// Sparse attributes of a WebAXObject whose value is a vector of
// references to other WebAXObjects within the same frame. In order
// for it to be a sparse attribute the default value must be the
// empty vector.
enum class WebAXObjectVectorAttribute {
  kAriaControls,
  kAriaFlowTo,
};

}  // namespace blink

#endif
