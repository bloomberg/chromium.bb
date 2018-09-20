// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/ax_enums.h"

#include "third_party/blink/public/web/web_ax_enums.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kActivate,
                   AXDefaultActionVerb::kActivate);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kCheck, AXDefaultActionVerb::kCheck);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kClick, AXDefaultActionVerb::kClick);
STATIC_ASSERT_ENUM(WebAXDefaultActionVerb::kClickAncestor,
                   AXDefaultActionVerb::kClickAncestor);
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

STATIC_ASSERT_ENUM(kWebAXTextPositionNone, kAXTextPositionNone);
STATIC_ASSERT_ENUM(kWebAXTextPositionSubscript, kAXTextPositionSubscript);
STATIC_ASSERT_ENUM(kWebAXTextPositionSuperscript, kAXTextPositionSuperscript);

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

STATIC_ASSERT_ENUM(kWebAXHasPopupFalse, kAXHasPopupFalse);
STATIC_ASSERT_ENUM(kWebAXHasPopupTrue, kAXHasPopupTrue);
STATIC_ASSERT_ENUM(kWebAXHasPopupMenu, kAXHasPopupMenu);
STATIC_ASSERT_ENUM(kWebAXHasPopupListbox, kAXHasPopupListbox);
STATIC_ASSERT_ENUM(kWebAXHasPopupTree, kAXHasPopupTree);
STATIC_ASSERT_ENUM(kWebAXHasPopupGrid, kAXHasPopupGrid);
STATIC_ASSERT_ENUM(kWebAXHasPopupDialog, kAXHasPopupDialog);

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
}  // namespace blink
