// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AXEnums_h
#define AXEnums_h

#include "core/CoreExport.h"

namespace blink {

enum AccessibilityRole {
  kUnknownRole = 0,  // Not mapped in platform APIs, generally indicates a bug
  kAbbrRole,         // No mapping to ARIA role.
  kAlertDialogRole,
  kAlertRole,
  kAnchorRole,      // No mapping to ARIA role.
  kAnnotationRole,  // No mapping to ARIA role.
  kApplicationRole,
  kArticleRole,
  kAudioRole,  // No mapping to ARIA role.
  kBannerRole,
  kBlockquoteRole,     // No mapping to ARIA role.
  kBusyIndicatorRole,  // No mapping to ARIA role.
  kButtonRole,
  kCanvasRole,   // No mapping to ARIA role.
  kCaptionRole,  // No mapping to ARIA role.
  kCellRole,
  kCheckBoxRole,
  kColorWellRole,  // No mapping to ARIA role.
  kColumnHeaderRole,
  kColumnRole,  // No mapping to ARIA role.
  kComboBoxRole,
  kComplementaryRole,
  kContentInfoRole,
  kDateRole,      // No mapping to ARIA role.
  kDateTimeRole,  // No mapping to ARIA role.
  kDefinitionRole,
  kDescriptionListDetailRole,  // No mapping to ARIA role.
  kDescriptionListRole,        // No mapping to ARIA role.
  kDescriptionListTermRole,    // No mapping to ARIA role.
  kDetailsRole,                // No mapping to ARIA role.
  kDialogRole,
  kDirectoryRole,
  kDisclosureTriangleRole,  // No mapping to ARIA role.
  kDocumentRole,
  kEmbeddedObjectRole,  // No mapping to ARIA role.
  kFeedRole,
  kFigcaptionRole,  // No mapping to ARIA role.
  kFigureRole,
  kFooterRole,
  kFormRole,
  kGenericContainerRole,  // No role was defined for this container
  kGridRole,
  kGroupRole,
  kHeadingRole,
  kIframePresentationalRole,  // No mapping to ARIA role.
  kIframeRole,                // No mapping to ARIA role.
  kIgnoredRole,               // No mapping to ARIA role.
  kImageMapLinkRole,          // No mapping to ARIA role.
  kImageMapRole,              // No mapping to ARIA role.
  kImageRole,
  kInlineTextBoxRole,  // No mapping to ARIA role.
  kInputTimeRole,      // No mapping to ARIA role.
  kLabelRole,
  kLegendRole,     // No mapping to ARIA role.
  kLineBreakRole,  // No mapping to ARIA role.
  kLinkRole,
  kListBoxOptionRole,
  kListBoxRole,
  kListItemRole,
  kListMarkerRole,  // No mapping to ARIA role.
  kListRole,
  kLogRole,
  kMainRole,
  kMarkRole,  // No mapping to ARIA role.
  kMarqueeRole,
  kMathRole,
  kMenuBarRole,
  kMenuButtonRole,
  kMenuItemRole,
  kMenuItemCheckBoxRole,
  kMenuItemRadioRole,
  kMenuListOptionRole,
  kMenuListPopupRole,
  kMenuRole,
  kMeterRole,
  kNavigationRole,
  kNoneRole,  // ARIA role of "none"
  kNoteRole,
  kOutlineRole,    // No mapping to ARIA role.
  kParagraphRole,  // No mapping to ARIA role.
  kPopUpButtonRole,
  kPreRole,  // No mapping to ARIA role.
  kPresentationalRole,
  kProgressIndicatorRole,
  kRadioButtonRole,
  kRadioGroupRole,
  kRegionRole,
  kRootWebAreaRole,  // No mapping to ARIA role.
  kRowHeaderRole,
  kRowRole,
  kRubyRole,        // No mapping to ARIA role.
  kRulerRole,       // No mapping to ARIA role.
  kSVGRootRole,     // No mapping to ARIA role.
  kScrollAreaRole,  // No mapping to ARIA role.
  kScrollBarRole,
  kSeamlessWebAreaRole,  // No mapping to ARIA role.
  kSearchRole,
  kSearchBoxRole,
  kSliderRole,
  kSliderThumbRole,     // No mapping to ARIA role.
  kSpinButtonPartRole,  // No mapping to ARIA role.
  kSpinButtonRole,
  kSplitterRole,
  kStaticTextRole,  // No mapping to ARIA role.
  kStatusRole,
  kSwitchRole,
  kTabGroupRole,  // No mapping to ARIA role.
  kTabListRole,
  kTabPanelRole,
  kTabRole,
  kTableHeaderContainerRole,  // No mapping to ARIA role.
  kTableRole,
  kTermRole,
  kTextFieldRole,
  kTimeRole,  // No mapping to ARIA role.
  kTimerRole,
  kToggleButtonRole,
  kToolbarRole,
  kTreeGridRole,
  kTreeItemRole,
  kTreeRole,
  kUserInterfaceTooltipRole,
  kVideoRole,    // No mapping to ARIA role.
  kWebAreaRole,  // No mapping to ARIA role.
  kWindowRole,   // No mapping to ARIA role.
  kNumRoles
};

enum AccessibilityState {
  kAXBusyState,
  kAXExpandedState,
  kAXFocusableState,
  kAXFocusedState,
  kAXHaspopupState,
  kAXHoveredState,
  kAXInvisibleState,
  kAXLinkedState,
  kAXMultilineState,
  kAXMultiselectableState,
  kAXOffscreenState,
  kAXProtectedState,
  kAXRequiredState,
  kAXSelectableState,
  kAXSelectedState,
  kAXVerticalState,
  kAXVisitedState
};

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

}  // namespace blink

#endif  // AXEnums_h
