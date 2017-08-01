// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/accessibility/AXEnums.h"

#include "core/HTMLElementTypeHelpers.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/StringHash.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebAXEnums.h"

namespace blink {

STATIC_ASSERT_ENUM(kWebAXRoleAbbr, kAbbrRole);
STATIC_ASSERT_ENUM(kWebAXRoleAlertDialog, kAlertDialogRole);
STATIC_ASSERT_ENUM(kWebAXRoleAlert, kAlertRole);
STATIC_ASSERT_ENUM(kWebAXRoleAnchor, kAnchorRole);
STATIC_ASSERT_ENUM(kWebAXRoleAnnotation, kAnnotationRole);
STATIC_ASSERT_ENUM(kWebAXRoleApplication, kApplicationRole);
STATIC_ASSERT_ENUM(kWebAXRoleArticle, kArticleRole);
STATIC_ASSERT_ENUM(kWebAXRoleAudio, kAudioRole);
STATIC_ASSERT_ENUM(kWebAXRoleBanner, kBannerRole);
STATIC_ASSERT_ENUM(kWebAXRoleBlockquote, kBlockquoteRole);
STATIC_ASSERT_ENUM(kWebAXRoleBusyIndicator, kBusyIndicatorRole);
STATIC_ASSERT_ENUM(kWebAXRoleButton, kButtonRole);
STATIC_ASSERT_ENUM(kWebAXRoleCanvas, kCanvasRole);
STATIC_ASSERT_ENUM(kWebAXRoleCaption, kCaptionRole);
STATIC_ASSERT_ENUM(kWebAXRoleCell, kCellRole);
STATIC_ASSERT_ENUM(kWebAXRoleCheckBox, kCheckBoxRole);
STATIC_ASSERT_ENUM(kWebAXRoleColorWell, kColorWellRole);
STATIC_ASSERT_ENUM(kWebAXRoleColumnHeader, kColumnHeaderRole);
STATIC_ASSERT_ENUM(kWebAXRoleColumn, kColumnRole);
STATIC_ASSERT_ENUM(kWebAXRoleComboBox, kComboBoxRole);
STATIC_ASSERT_ENUM(kWebAXRoleComplementary, kComplementaryRole);
STATIC_ASSERT_ENUM(kWebAXRoleContentInfo, kContentInfoRole);
STATIC_ASSERT_ENUM(kWebAXRoleDate, kDateRole);
STATIC_ASSERT_ENUM(kWebAXRoleDateTime, kDateTimeRole);
STATIC_ASSERT_ENUM(kWebAXRoleDefinition, kDefinitionRole);
STATIC_ASSERT_ENUM(kWebAXRoleDescriptionListDetail, kDescriptionListDetailRole);
STATIC_ASSERT_ENUM(kWebAXRoleDescriptionList, kDescriptionListRole);
STATIC_ASSERT_ENUM(kWebAXRoleDescriptionListTerm, kDescriptionListTermRole);
STATIC_ASSERT_ENUM(kWebAXRoleDetails, kDetailsRole);
STATIC_ASSERT_ENUM(kWebAXRoleDialog, kDialogRole);
STATIC_ASSERT_ENUM(kWebAXRoleDirectory, kDirectoryRole);
STATIC_ASSERT_ENUM(kWebAXRoleDisclosureTriangle, kDisclosureTriangleRole);
STATIC_ASSERT_ENUM(kWebAXRoleDocument, kDocumentRole);
STATIC_ASSERT_ENUM(kWebAXRoleEmbeddedObject, kEmbeddedObjectRole);
STATIC_ASSERT_ENUM(kWebAXRoleFeed, kFeedRole);
STATIC_ASSERT_ENUM(kWebAXRoleFigcaption, kFigcaptionRole);
STATIC_ASSERT_ENUM(kWebAXRoleFigure, kFigureRole);
STATIC_ASSERT_ENUM(kWebAXRoleFooter, kFooterRole);
STATIC_ASSERT_ENUM(kWebAXRoleForm, kFormRole);
STATIC_ASSERT_ENUM(kWebAXRoleGenericContainer, kGenericContainerRole);
STATIC_ASSERT_ENUM(kWebAXRoleGrid, kGridRole);
STATIC_ASSERT_ENUM(kWebAXRoleGroup, kGroupRole);
STATIC_ASSERT_ENUM(kWebAXRoleHeading, kHeadingRole);
STATIC_ASSERT_ENUM(kWebAXRoleIframe, kIframeRole);
STATIC_ASSERT_ENUM(kWebAXRoleIframePresentational, kIframePresentationalRole);
STATIC_ASSERT_ENUM(kWebAXRoleIgnored, kIgnoredRole);
STATIC_ASSERT_ENUM(kWebAXRoleImageMapLink, kImageMapLinkRole);
STATIC_ASSERT_ENUM(kWebAXRoleImageMap, kImageMapRole);
STATIC_ASSERT_ENUM(kWebAXRoleImage, kImageRole);
STATIC_ASSERT_ENUM(kWebAXRoleInlineTextBox, kInlineTextBoxRole);
STATIC_ASSERT_ENUM(kWebAXRoleInputTime, kInputTimeRole);
STATIC_ASSERT_ENUM(kWebAXRoleLabel, kLabelRole);
STATIC_ASSERT_ENUM(kWebAXRoleLegend, kLegendRole);
STATIC_ASSERT_ENUM(kWebAXRoleLineBreak, kLineBreakRole);
STATIC_ASSERT_ENUM(kWebAXRoleLink, kLinkRole);
STATIC_ASSERT_ENUM(kWebAXRoleListBoxOption, kListBoxOptionRole);
STATIC_ASSERT_ENUM(kWebAXRoleListBox, kListBoxRole);
STATIC_ASSERT_ENUM(kWebAXRoleListItem, kListItemRole);
STATIC_ASSERT_ENUM(kWebAXRoleListMarker, kListMarkerRole);
STATIC_ASSERT_ENUM(kWebAXRoleList, kListRole);
STATIC_ASSERT_ENUM(kWebAXRoleLog, kLogRole);
STATIC_ASSERT_ENUM(kWebAXRoleMain, kMainRole);
STATIC_ASSERT_ENUM(kWebAXRoleMark, kMarkRole);
STATIC_ASSERT_ENUM(kWebAXRoleMarquee, kMarqueeRole);
STATIC_ASSERT_ENUM(kWebAXRoleMath, kMathRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenuBar, kMenuBarRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenuButton, kMenuButtonRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenuItem, kMenuItemRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenuItemCheckBox, kMenuItemCheckBoxRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenuItemRadio, kMenuItemRadioRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenuListOption, kMenuListOptionRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenuListPopup, kMenuListPopupRole);
STATIC_ASSERT_ENUM(kWebAXRoleMenu, kMenuRole);
STATIC_ASSERT_ENUM(kWebAXRoleMeter, kMeterRole);
STATIC_ASSERT_ENUM(kWebAXRoleNavigation, kNavigationRole);
STATIC_ASSERT_ENUM(kWebAXRoleNone, kNoneRole);
STATIC_ASSERT_ENUM(kWebAXRoleNote, kNoteRole);
STATIC_ASSERT_ENUM(kWebAXRoleOutline, kOutlineRole);
STATIC_ASSERT_ENUM(kWebAXRoleParagraph, kParagraphRole);
STATIC_ASSERT_ENUM(kWebAXRolePopUpButton, kPopUpButtonRole);
STATIC_ASSERT_ENUM(kWebAXRolePre, kPreRole);
STATIC_ASSERT_ENUM(kWebAXRolePresentational, kPresentationalRole);
STATIC_ASSERT_ENUM(kWebAXRoleProgressIndicator, kProgressIndicatorRole);
STATIC_ASSERT_ENUM(kWebAXRoleRadioButton, kRadioButtonRole);
STATIC_ASSERT_ENUM(kWebAXRoleRadioGroup, kRadioGroupRole);
STATIC_ASSERT_ENUM(kWebAXRoleRegion, kRegionRole);
STATIC_ASSERT_ENUM(kWebAXRoleRootWebArea, kRootWebAreaRole);
STATIC_ASSERT_ENUM(kWebAXRoleRowHeader, kRowHeaderRole);
STATIC_ASSERT_ENUM(kWebAXRoleRow, kRowRole);
STATIC_ASSERT_ENUM(kWebAXRoleRuby, kRubyRole);
STATIC_ASSERT_ENUM(kWebAXRoleRuler, kRulerRole);
STATIC_ASSERT_ENUM(kWebAXRoleSVGRoot, kSVGRootRole);
STATIC_ASSERT_ENUM(kWebAXRoleScrollArea, kScrollAreaRole);
STATIC_ASSERT_ENUM(kWebAXRoleScrollBar, kScrollBarRole);
STATIC_ASSERT_ENUM(kWebAXRoleSeamlessWebArea, kSeamlessWebAreaRole);
STATIC_ASSERT_ENUM(kWebAXRoleSearch, kSearchRole);
STATIC_ASSERT_ENUM(kWebAXRoleSearchBox, kSearchBoxRole);
STATIC_ASSERT_ENUM(kWebAXRoleSlider, kSliderRole);
STATIC_ASSERT_ENUM(kWebAXRoleSliderThumb, kSliderThumbRole);
STATIC_ASSERT_ENUM(kWebAXRoleSpinButtonPart, kSpinButtonPartRole);
STATIC_ASSERT_ENUM(kWebAXRoleSpinButton, kSpinButtonRole);
STATIC_ASSERT_ENUM(kWebAXRoleSplitter, kSplitterRole);
STATIC_ASSERT_ENUM(kWebAXRoleStaticText, kStaticTextRole);
STATIC_ASSERT_ENUM(kWebAXRoleStatus, kStatusRole);
STATIC_ASSERT_ENUM(kWebAXRoleSwitch, kSwitchRole);
STATIC_ASSERT_ENUM(kWebAXRoleTabGroup, kTabGroupRole);
STATIC_ASSERT_ENUM(kWebAXRoleTabList, kTabListRole);
STATIC_ASSERT_ENUM(kWebAXRoleTabPanel, kTabPanelRole);
STATIC_ASSERT_ENUM(kWebAXRoleTab, kTabRole);
STATIC_ASSERT_ENUM(kWebAXRoleTableHeaderContainer, kTableHeaderContainerRole);
STATIC_ASSERT_ENUM(kWebAXRoleTable, kTableRole);
STATIC_ASSERT_ENUM(kWebAXRoleTerm, kTermRole);
STATIC_ASSERT_ENUM(kWebAXRoleTextField, kTextFieldRole);
STATIC_ASSERT_ENUM(kWebAXRoleTime, kTimeRole);
STATIC_ASSERT_ENUM(kWebAXRoleTimer, kTimerRole);
STATIC_ASSERT_ENUM(kWebAXRoleToggleButton, kToggleButtonRole);
STATIC_ASSERT_ENUM(kWebAXRoleToolbar, kToolbarRole);
STATIC_ASSERT_ENUM(kWebAXRoleTreeGrid, kTreeGridRole);
STATIC_ASSERT_ENUM(kWebAXRoleTreeItem, kTreeItemRole);
STATIC_ASSERT_ENUM(kWebAXRoleTree, kTreeRole);
STATIC_ASSERT_ENUM(kWebAXRoleUnknown, kUnknownRole);
STATIC_ASSERT_ENUM(kWebAXRoleUserInterfaceTooltip, kUserInterfaceTooltipRole);
STATIC_ASSERT_ENUM(kWebAXRoleVideo, kVideoRole);
STATIC_ASSERT_ENUM(kWebAXRoleWebArea, kWebAreaRole);
STATIC_ASSERT_ENUM(kWebAXRoleWindow, kWindowRole);

STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kNone, AXDefaultActionVerb::kNone);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kActivate,
                   AXDefaultActionVerb::kActivate);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kCheck, AXDefaultActionVerb::kCheck);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kClick, AXDefaultActionVerb::kClick);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kJump, AXDefaultActionVerb::kJump);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kOpen, AXDefaultActionVerb::kOpen);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kPress, AXDefaultActionVerb::kPress);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kSelect,
                   AXDefaultActionVerb::kSelect);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kUncheck,
                   AXDefaultActionVerb::kUncheck);

STATIC_ASSERT_ENUM(kWebAXTextDirectionLR, kAccessibilityTextDirectionLTR);
STATIC_ASSERT_ENUM(kWebAXTextDirectionRL, kAccessibilityTextDirectionRTL);
STATIC_ASSERT_ENUM(kWebAXTextDirectionTB, kAccessibilityTextDirectionTTB);
STATIC_ASSERT_ENUM(kWebAXTextDirectionBT, kAccessibilityTextDirectionBTT);

STATIC_ASSERT_ENUM(kWebAXSortDirectionUndefined, kSortDirectionUndefined);
STATIC_ASSERT_ENUM(kWebAXSortDirectionNone, kSortDirectionNone);
STATIC_ASSERT_ENUM(kWebAXSortDirectionAscending, kSortDirectionAscending);
STATIC_ASSERT_ENUM(kWebAXSortDirectionDescending, kSortDirectionDescending);
STATIC_ASSERT_ENUM(kWebAXSortDirectionOther, kSortDirectionOther);

STATIC_ASSERT_ENUM(kWebAXExpandedUndefined, kExpandedUndefined);
STATIC_ASSERT_ENUM(kWebAXExpandedCollapsed, kExpandedCollapsed);
STATIC_ASSERT_ENUM(kWebAXExpandedExpanded, kExpandedExpanded);

STATIC_ASSERT_ENUM(kWebAXOrientationUndefined,
                   kAccessibilityOrientationUndefined);
STATIC_ASSERT_ENUM(kWebAXOrientationVertical,
                   kAccessibilityOrientationVertical);
STATIC_ASSERT_ENUM(kWebAXOrientationHorizontal,
                   kAccessibilityOrientationHorizontal);

STATIC_ASSERT_ENUM(kWebAXAriaCurrentStateUndefined, kAriaCurrentStateUndefined);
STATIC_ASSERT_ENUM(kWebAXAriaCurrentStateFalse, kAriaCurrentStateFalse);
STATIC_ASSERT_ENUM(kWebAXAriaCurrentStateTrue, kAriaCurrentStateTrue);
STATIC_ASSERT_ENUM(kWebAXAriaCurrentStatePage, kAriaCurrentStatePage);
STATIC_ASSERT_ENUM(kWebAXAriaCurrentStateStep, kAriaCurrentStateStep);
STATIC_ASSERT_ENUM(kWebAXAriaCurrentStateLocation, kAriaCurrentStateLocation);
STATIC_ASSERT_ENUM(kWebAXAriaCurrentStateDate, kAriaCurrentStateDate);
STATIC_ASSERT_ENUM(kWebAXAriaCurrentStateTime, kAriaCurrentStateTime);

STATIC_ASSERT_ENUM(kWebAXInvalidStateUndefined, kInvalidStateUndefined);
STATIC_ASSERT_ENUM(kWebAXInvalidStateFalse, kInvalidStateFalse);
STATIC_ASSERT_ENUM(kWebAXInvalidStateTrue, kInvalidStateTrue);
STATIC_ASSERT_ENUM(kWebAXInvalidStateSpelling, kInvalidStateSpelling);
STATIC_ASSERT_ENUM(kWebAXInvalidStateGrammar, kInvalidStateGrammar);
STATIC_ASSERT_ENUM(kWebAXInvalidStateOther, kInvalidStateOther);

STATIC_ASSERT_ENUM(kWebAXTextStyleNone, kTextStyleNone);
STATIC_ASSERT_ENUM(kWebAXTextStyleBold, kTextStyleBold);
STATIC_ASSERT_ENUM(kWebAXTextStyleItalic, kTextStyleItalic);
STATIC_ASSERT_ENUM(kWebAXTextStyleUnderline, kTextStyleUnderline);
STATIC_ASSERT_ENUM(kWebAXTextStyleLineThrough, kTextStyleLineThrough);

STATIC_ASSERT_ENUM(kWebAXNameFromUninitialized, kAXNameFromUninitialized);
STATIC_ASSERT_ENUM(kWebAXNameFromAttribute, kAXNameFromAttribute);
STATIC_ASSERT_ENUM(kWebAXNameFromAttributeExplicitlyEmpty,
                   kAXNameFromAttributeExplicitlyEmpty);
STATIC_ASSERT_ENUM(kWebAXNameFromCaption, kAXNameFromCaption);
STATIC_ASSERT_ENUM(kWebAXNameFromContents, kAXNameFromContents);
STATIC_ASSERT_ENUM(kWebAXNameFromPlaceholder, kAXNameFromPlaceholder);
STATIC_ASSERT_ENUM(kWebAXNameFromRelatedElement, kAXNameFromRelatedElement);
STATIC_ASSERT_ENUM(kWebAXNameFromValue, kAXNameFromValue);
STATIC_ASSERT_ENUM(kWebAXNameFromTitle, kAXNameFromTitle);

STATIC_ASSERT_ENUM(kWebAXDescriptionFromUninitialized,
                   kAXDescriptionFromUninitialized);
STATIC_ASSERT_ENUM(kWebAXDescriptionFromAttribute, kAXDescriptionFromAttribute);
STATIC_ASSERT_ENUM(kWebAXDescriptionFromContents, kAXDescriptionFromContents);
STATIC_ASSERT_ENUM(kWebAXDescriptionFromRelatedElement,
                   kAXDescriptionFromRelatedElement);

STATIC_ASSERT_ENUM(WebAXStringAttribute::kAriaKeyShortcuts,
                   AXStringAttribute::kAriaKeyShortcuts);
STATIC_ASSERT_ENUM(WebAXStringAttribute::kAriaRoleDescription,
                   AXStringAttribute::kAriaRoleDescription);
STATIC_ASSERT_ENUM(WebAXObjectAttribute::kAriaActiveDescendant,
                   AXObjectAttribute::kAriaActiveDescendant);
STATIC_ASSERT_ENUM(WebAXObjectAttribute::kAriaErrorMessage,
                   AXObjectAttribute::kAriaErrorMessage);
STATIC_ASSERT_ENUM(WebAXObjectVectorAttribute::kAriaControls,
                   AXObjectVectorAttribute::kAriaControls);
STATIC_ASSERT_ENUM(WebAXObjectAttribute::kAriaDetails,
                   AXObjectAttribute::kAriaDetails);
STATIC_ASSERT_ENUM(WebAXObjectVectorAttribute::kAriaFlowTo,
                   AXObjectVectorAttribute::kAriaFlowTo);
#undef STATIC_ASSERT_ENUM
}  // namespace blink
