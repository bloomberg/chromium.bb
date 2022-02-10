// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/accessibility/browser_accessibility_cocoa.h"

#include <execinfo.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <utility>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/public/common/content_client.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/strings/grit/ax_strings.h"

#import "ui/accessibility/platform/ax_platform_node_mac.h"

using StringAttribute = ax::mojom::StringAttribute;
using content::AccessibilityMatchPredicate;
using content::BrowserAccessibility;
using content::BrowserAccessibilityDelegate;
using content::BrowserAccessibilityManager;
using content::BrowserAccessibilityManagerMac;
using content::ContentClient;
using content::IsUseZoomForDSFEnabled;
using content::OneShotAccessibilityTreeSearch;
using ui::AXNodeData;
using ui::AXActionHandlerRegistry;

static_assert(
    std::is_trivially_copyable<BrowserAccessibility::SerializedPosition>::value,
    "BrowserAccessibility::SerializedPosition must be POD because it's used to "
    "back an AXTextMarker");

namespace {

// Private WebKit accessibility attributes.
NSString* const NSAccessibilityBlockQuoteLevelAttribute = @"AXBlockQuoteLevel";
NSString* const NSAccessibilityDOMClassList = @"AXDOMClassList";
NSString* const NSAccessibilityDropEffectsAttribute = @"AXDropEffects";
NSString* const NSAccessibilityEditableAncestorAttribute =
    @"AXEditableAncestor";
NSString* const NSAccessibilityElementBusyAttribute = @"AXElementBusy";
NSString* const NSAccessibilityFocusableAncestorAttribute =
    @"AXFocusableAncestor";
NSString* const NSAccessibilityGrabbedAttribute = @"AXGrabbed";
NSString* const NSAccessibilityHighestEditableAncestorAttribute =
    @"AXHighestEditableAncestor";
NSString* const NSAccessibilityIsMultiSelectableAttribute =
    @"AXIsMultiSelectable";
NSString* const NSAccessibilityLoadingProgressAttribute = @"AXLoadingProgress";
NSString* const NSAccessibilityOwnsAttribute = @"AXOwns";
NSString* const
    NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute =
        @"AXUIElementCountForSearchPredicate";
NSString* const
    NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute =
        @"AXUIElementsForSearchPredicate";
NSString* const NSAccessibilityVisitedAttribute = @"AXVisited";
NSString* const NSAccessibilityKeyShortcutsValueAttribute =
    @"AXKeyShortcutsValue";

// Private attributes for text markers.
NSString* const NSAccessibilityStartTextMarkerAttribute = @"AXStartTextMarker";
NSString* const NSAccessibilityEndTextMarkerAttribute = @"AXEndTextMarker";
NSString* const NSAccessibilitySelectedTextMarkerRangeAttribute =
    @"AXSelectedTextMarkerRange";
NSString* const NSAccessibilityTextMarkerIsValidParameterizedAttribute =
    @"AXTextMarkerIsValid";
NSString* const NSAccessibilityIndexForTextMarkerParameterizedAttribute =
    @"AXIndexForTextMarker";
NSString* const NSAccessibilityTextMarkerForIndexParameterizedAttribute =
    @"AXTextMarkerForIndex";
NSString* const NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute =
    @"AXEndTextMarkerForBounds";
NSString* const NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute =
    @"AXStartTextMarkerForBounds";
NSString* const
    NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLineTextMarkerRangeForTextMarker";
// TODO(nektar): Implement programmatic text operations.
//
// NSString* const NSAccessibilityTextOperationMarkerRanges =
//    @"AXTextOperationMarkerRanges";
NSString* const NSAccessibilityUIElementForTextMarkerParameterizedAttribute =
    @"AXUIElementForTextMarker";
NSString* const
    NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute =
        @"AXTextMarkerRangeForUIElement";
NSString* const NSAccessibilityLineForTextMarkerParameterizedAttribute =
    @"AXLineForTextMarker";
NSString* const NSAccessibilityTextMarkerRangeForLineParameterizedAttribute =
    @"AXTextMarkerRangeForLine";
NSString* const NSAccessibilityStringForTextMarkerRangeParameterizedAttribute =
    @"AXStringForTextMarkerRange";
NSString* const NSAccessibilityTextMarkerForPositionParameterizedAttribute =
    @"AXTextMarkerForPosition";
NSString* const NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute =
    @"AXBoundsForTextMarkerRange";
NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRange";
NSString* const
    NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute =
        @"AXAttributedStringForTextMarkerRangeWithOptions";
NSString* const
    NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute =
        @"AXTextMarkerRangeForUnorderedTextMarkers";
NSString* const
    NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousTextMarkerForTextMarker";
NSString* const
    NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftWordTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightWordTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXLeftLineTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXRightLineTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXSentenceTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXParagraphTextMarkerRangeForTextMarker";
NSString* const
    NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextWordEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousWordStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextLineEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousLineStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextSentenceEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousSentenceStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute =
        @"AXNextParagraphEndTextMarkerForTextMarker";
NSString* const
    NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute =
        @"AXPreviousParagraphStartTextMarkerForTextMarker";
NSString* const
    NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute =
        @"AXStyleTextMarkerRangeForTextMarker";
NSString* const NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute =
    @"AXLengthForTextMarkerRange";

// Private attributes that can be used for testing text markers, e.g. in dump
// tree tests.
NSString* const
    NSAccessibilityTextMarkerDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerDebugDescription";
NSString* const
    NSAccessibilityTextMarkerRangeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerRangeDebugDescription";
NSString* const
    NSAccessibilityTextMarkerNodeDebugDescriptionParameterizedAttribute =
        @"AXTextMarkerNodeDebugDescription";

// Other private attributes.
NSString* const NSAccessibilityIdentifierChromeAttribute = @"ChromeAXNodeId";
NSString* const NSAccessibilitySelectTextWithCriteriaParameterizedAttribute =
    @"AXSelectTextWithCriteria";
NSString* const NSAccessibilityIndexForChildUIElementParameterizedAttribute =
    @"AXIndexForChildUIElement";
NSString* const NSAccessibilityValueAutofillAvailableAttribute =
    @"AXValueAutofillAvailable";
// Not currently supported by Chrome -- information not stored:
// NSString* const NSAccessibilityValueAutofilledAttribute =
// @"AXValueAutofilled"; Not currently supported by Chrome -- mismatch of types
// supported: NSString* const NSAccessibilityValueAutofillTypeAttribute =
// @"AXValueAutofillType";

// Actions.
NSString* const NSAccessibilityScrollToVisibleAction = @"AXScrollToVisible";

// A mapping from an accessibility attribute to its method name.
NSDictionary* attributeToMethodNameMap = nil;

// VoiceOver uses -1 to mean "no limit" for AXResultsLimit.
const int kAXResultsLimitNoLimit = -1;

// The following are private accessibility APIs required for cursor navigation
// and text selection. VoiceOver started relying on them in Mac OS X 10.11.
// They are public as of the 12.0 SDK.
#if !defined(MAC_OS_VERSION_12_0) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_12_0
using AXTextMarkerRangeRef = CFTypeRef;
using AXTextMarkerRef = CFTypeRef;
extern "C" {
CFTypeID AXTextMarkerGetTypeID();
AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef,
                                   const UInt8* bytes,
                                   CFIndex length);
size_t AXTextMarkerGetLength(AXTextMarkerRef);
const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef);

CFTypeID AXTextMarkerRangeGetTypeID();
AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef,
                                             AXTextMarkerRef start,
                                             AXTextMarkerRef end);
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef);
AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef);
}  // extern "C"
#endif

// AXTextMarkerCreate is a system function that makes a copy of the data buffer
// given to it.
id CreateTextMarker(BrowserAccessibility::AXPosition position) {
  BrowserAccessibility::SerializedPosition serialized = position->Serialize();
  AXTextMarkerRef cf_text_marker = AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized),
      sizeof(BrowserAccessibility::SerializedPosition));
  return [static_cast<id>(cf_text_marker) autorelease];
}

id CreateTextMarkerRange(const BrowserAccessibility::AXRange range) {
  BrowserAccessibility::SerializedPosition serialized_anchor =
      range.anchor()->Serialize();
  BrowserAccessibility::SerializedPosition serialized_focus =
      range.focus()->Serialize();
  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_anchor),
      sizeof(BrowserAccessibility::SerializedPosition)));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(AXTextMarkerCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(&serialized_focus),
      sizeof(BrowserAccessibility::SerializedPosition)));
  AXTextMarkerRangeRef cf_marker_range =
      AXTextMarkerRangeCreate(kCFAllocatorDefault, start_marker, end_marker);
  return [static_cast<id>(cf_marker_range) autorelease];
}

BrowserAccessibility::AXPosition CreatePositionFromTextMarker(id text_marker) {
  if (!content::IsAXTextMarker(text_marker))
    return ui::AXNodePosition::CreateNullPosition();

  AXTextMarkerRef cf_text_marker = static_cast<AXTextMarkerRef>(text_marker);
  if (AXTextMarkerGetLength(cf_text_marker) !=
      sizeof(BrowserAccessibility::SerializedPosition))
    return ui::AXNodePosition::CreateNullPosition();

  const UInt8* source_buffer = AXTextMarkerGetBytePtr(cf_text_marker);
  if (!source_buffer)
    return ui::AXNodePosition::CreateNullPosition();

  return ui::AXNodePosition::Unserialize(
      *reinterpret_cast<const BrowserAccessibility::SerializedPosition*>(
          source_buffer));
}

BrowserAccessibility::AXRange CreateRangeFromTextMarkerRange(id marker_range) {
  if (!content::IsAXTextMarkerRange(marker_range)) {
    return BrowserAccessibility::AXRange();
  }

  AXTextMarkerRangeRef cf_marker_range =
      static_cast<AXTextMarkerRangeRef>(marker_range);

  base::ScopedCFTypeRef<AXTextMarkerRef> start_marker(
      AXTextMarkerRangeCopyStartMarker(cf_marker_range));
  base::ScopedCFTypeRef<AXTextMarkerRef> end_marker(
      AXTextMarkerRangeCopyEndMarker(cf_marker_range));
  if (!start_marker.get() || !end_marker.get())
    return BrowserAccessibility::AXRange();

  BrowserAccessibility::AXPosition anchor =
      CreatePositionFromTextMarker(static_cast<id>(start_marker.get()));
  BrowserAccessibility::AXPosition focus =
      CreatePositionFromTextMarker(static_cast<id>(end_marker.get()));
  // |BrowserAccessibility::AXRange| takes ownership of its anchor and focus.
  return BrowserAccessibility::AXRange(std::move(anchor), std::move(focus));
}

BrowserAccessibility::AXPosition CreateTreePosition(
    const BrowserAccessibility& object,
    int offset) {
  const BrowserAccessibilityManager* manager = object.manager();
  DCHECK(manager);
  return ui::AXNodePosition::CreateTreePosition(manager->ax_tree_id(),
                                                object.GetId(), offset);
}

BrowserAccessibility::AXPosition CreateTextPosition(
    const BrowserAccessibility& object,
    int offset,
    ax::mojom::TextAffinity affinity) {
  const BrowserAccessibilityManager* manager = object.manager();
  DCHECK(manager);
  return ui::AXNodePosition::CreateTextPosition(
      manager->ax_tree_id(), object.GetId(), offset, affinity);
}

BrowserAccessibility::AXRange CreateAXRange(
    const BrowserAccessibility& start_object,
    int start_offset,
    ax::mojom::TextAffinity start_affinity,
    const BrowserAccessibility& end_object,
    int end_offset,
    ax::mojom::TextAffinity end_affinity) {
  BrowserAccessibility::AXPosition anchor =
      start_object.IsLeaf()
          ? CreateTextPosition(start_object, start_offset, start_affinity)
          : CreateTreePosition(start_object, start_offset);
  BrowserAccessibility::AXPosition focus =
      end_object.IsLeaf()
          ? CreateTextPosition(end_object, end_offset, end_affinity)
          : CreateTreePosition(end_object, end_offset);
  // |BrowserAccessibility::AXRange| takes ownership of its anchor and focus.
  return BrowserAccessibility::AXRange(std::move(anchor), std::move(focus));
}

BrowserAccessibility::AXRange GetSelectedRange(BrowserAccessibility& owner) {
  const BrowserAccessibilityManager* manager = owner.manager();
  if (!manager)
    return {};

  const ui::AXTree::Selection unignored_selection =
      manager->ax_tree()->GetUnignoredSelection();
  int32_t anchor_id = unignored_selection.anchor_object_id;
  const BrowserAccessibility* anchor_object = manager->GetFromID(anchor_id);
  if (!anchor_object)
    return {};

  int32_t focus_id = unignored_selection.focus_object_id;
  const BrowserAccessibility* focus_object = manager->GetFromID(focus_id);
  if (!focus_object)
    return {};

  // |anchor_offset| and / or |focus_offset| refer to a character offset if
  // |anchor_object| / |focus_object| are text-only objects or atomic text
  // fields. Otherwise, they should be treated as child indices. An atomic text
  // field does not expose its internal implementation to assistive software,
  // appearing as a single leaf node in the accessibility tree. It includes
  // <input>, <textarea> and Views-based text fields.
  int anchor_offset = unignored_selection.anchor_offset;
  int focus_offset = unignored_selection.focus_offset;
  DCHECK_GE(anchor_offset, 0);
  DCHECK_GE(focus_offset, 0);

  ax::mojom::TextAffinity anchor_affinity = unignored_selection.anchor_affinity;
  ax::mojom::TextAffinity focus_affinity = unignored_selection.focus_affinity;

  return CreateAXRange(*anchor_object, anchor_offset, anchor_affinity,
                       *focus_object, focus_offset, focus_affinity);
}

void AddMisspelledTextAttributes(const BrowserAccessibility::AXRange& ax_range,
                                 NSMutableAttributedString* attributed_string) {
  int anchor_start_offset = 0;
  [attributed_string beginEditing];
  for (const BrowserAccessibility::AXRange& leaf_text_range : ax_range) {
    DCHECK(!leaf_text_range.IsNull());
    DCHECK_EQ(leaf_text_range.anchor()->GetAnchor(),
              leaf_text_range.focus()->GetAnchor())
        << "An anchor range should only span a single object.";

    auto* manager =
        BrowserAccessibilityManager::FromID(leaf_text_range.focus()->tree_id());
    DCHECK(manager) << "A non-null position should have an associated AX tree.";
    const BrowserAccessibility* anchor =
        manager->GetFromID(leaf_text_range.focus()->anchor_id());
    DCHECK(anchor) << "A non-null position should have a non-null anchor node.";
    const std::vector<int32_t>& marker_types =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerTypes);
    const std::vector<int>& marker_starts =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerStarts);
    const std::vector<int>& marker_ends =
        anchor->GetIntListAttribute(ax::mojom::IntListAttribute::kMarkerEnds);
    for (size_t i = 0; i < marker_types.size(); ++i) {
      if (!(marker_types[i] &
            static_cast<int32_t>(ax::mojom::MarkerType::kSpelling))) {
        continue;
      }

      int misspelling_start = anchor_start_offset + marker_starts[i];
      int misspelling_end = anchor_start_offset + marker_ends[i];
      int misspelling_length = misspelling_end - misspelling_start;
      DCHECK_LE(static_cast<unsigned long>(misspelling_end),
                [attributed_string length]);
      DCHECK_GT(misspelling_length, 0);
      [attributed_string
          addAttribute:NSAccessibilityMarkedMisspelledTextAttribute
                 value:@YES
                 range:NSMakeRange(misspelling_start, misspelling_length)];
    }

    anchor_start_offset += leaf_text_range.GetText().length();
  }
  [attributed_string endEditing];
}

NSString* GetTextForTextMarkerRange(id marker_range) {
  BrowserAccessibility::AXRange range =
      CreateRangeFromTextMarkerRange(marker_range);
  if (range.IsNull())
    return nil;
  return base::SysUTF16ToNSString(range.GetText());
}

NSAttributedString* GetAttributedTextForTextMarkerRange(id marker_range) {
  BrowserAccessibility::AXRange ax_range =
      CreateRangeFromTextMarkerRange(marker_range);
  if (ax_range.IsNull())
    return nil;

  NSString* text = base::SysUTF16ToNSString(ax_range.GetText());
  if ([text length] == 0)
    return nil;

  NSMutableAttributedString* attributed_text =
      [[[NSMutableAttributedString alloc] initWithString:text] autorelease];
  // Currently, we only decorate the attributed string with misspelling
  // information.
  AddMisspelledTextAttributes(ax_range, attributed_text);
  return attributed_text;
}

// Returns an autoreleased copy of the AXNodeData's attribute.
NSString* NSStringForStringAttribute(BrowserAccessibility* browserAccessibility,
                                     StringAttribute attribute) {
  return base::SysUTF8ToNSString(
      browserAccessibility->GetStringAttribute(attribute));
}

// GetState checks the bitmask used in AXNodeData to check
// if the given state was set on the accessibility object.
bool GetState(BrowserAccessibility* accessibility, ax::mojom::State state) {
  return accessibility->HasState(state);
}

// Given a search key provided to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate, return a predicate that can be added
// to OneShotAccessibilityTreeSearch.
AccessibilityMatchPredicate PredicateForSearchKey(NSString* searchKey) {
  if ([searchKey isEqualToString:@"AXAnyTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return true;
    };
  } else if ([searchKey isEqualToString:@"AXBlockquoteSameLevelSearchKey"]) {
    // TODO(dmazzoni): implement the "same level" part.
    return content::AccessibilityBlockquotePredicate;
  } else if ([searchKey isEqualToString:@"AXBlockquoteSearchKey"]) {
    return content::AccessibilityBlockquotePredicate;
  } else if ([searchKey isEqualToString:@"AXBoldFontSearchKey"]) {
    return content::AccessibilityTextStyleBoldPredicate;
  } else if ([searchKey isEqualToString:@"AXButtonSearchKey"]) {
    return content::AccessibilityButtonPredicate;
  } else if ([searchKey isEqualToString:@"AXCheckBoxSearchKey"]) {
    return content::AccessibilityCheckboxPredicate;
  } else if ([searchKey isEqualToString:@"AXControlSearchKey"]) {
    return content::AccessibilityControlPredicate;
  } else if ([searchKey isEqualToString:@"AXDifferentTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() != start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXFontChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFontColorChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXFrameSearchKey"]) {
    return content::AccessibilityFramePredicate;
  } else if ([searchKey isEqualToString:@"AXGraphicSearchKey"]) {
    return content::AccessibilityGraphicPredicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel1SearchKey"]) {
    return content::AccessibilityH1Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel2SearchKey"]) {
    return content::AccessibilityH2Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel3SearchKey"]) {
    return content::AccessibilityH3Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel4SearchKey"]) {
    return content::AccessibilityH4Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel5SearchKey"]) {
    return content::AccessibilityH5Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingLevel6SearchKey"]) {
    return content::AccessibilityH6Predicate;
  } else if ([searchKey isEqualToString:@"AXHeadingSameLevelSearchKey"]) {
    return content::AccessibilityHeadingSameLevelPredicate;
  } else if ([searchKey isEqualToString:@"AXHeadingSearchKey"]) {
    return content::AccessibilityHeadingPredicate;
  } else if ([searchKey isEqualToString:@"AXHighlightedSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXItalicFontSearchKey"]) {
    return content::AccessibilityTextStyleItalicPredicate;
  } else if ([searchKey isEqualToString:@"AXLandmarkSearchKey"]) {
    return content::AccessibilityLandmarkPredicate;
  } else if ([searchKey isEqualToString:@"AXLinkSearchKey"]) {
    return content::AccessibilityLinkPredicate;
  } else if ([searchKey isEqualToString:@"AXListSearchKey"]) {
    return content::AccessibilityListPredicate;
  } else if ([searchKey isEqualToString:@"AXLiveRegionSearchKey"]) {
    return content::AccessibilityLiveRegionPredicate;
  } else if ([searchKey isEqualToString:@"AXMisspelledWordSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXOutlineSearchKey"]) {
    return content::AccessibilityTreePredicate;
  } else if ([searchKey isEqualToString:@"AXPlainTextSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXRadioGroupSearchKey"]) {
    return content::AccessibilityRadioGroupPredicate;
  } else if ([searchKey isEqualToString:@"AXSameTypeSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->GetRole() == start->GetRole();
    };
  } else if ([searchKey isEqualToString:@"AXStaticTextSearchKey"]) {
    return [](BrowserAccessibility* start, BrowserAccessibility* current) {
      return current->IsText();
    };
  } else if ([searchKey isEqualToString:@"AXStyleChangeSearchKey"]) {
    // TODO(dmazzoni): implement this.
    return nullptr;
  } else if ([searchKey isEqualToString:@"AXTableSameLevelSearchKey"]) {
    // TODO(dmazzoni): implement the "same level" part.
    return content::AccessibilityTablePredicate;
  } else if ([searchKey isEqualToString:@"AXTableSearchKey"]) {
    return content::AccessibilityTablePredicate;
  } else if ([searchKey isEqualToString:@"AXTextFieldSearchKey"]) {
    return content::AccessibilityTextfieldPredicate;
  } else if ([searchKey isEqualToString:@"AXUnderlineSearchKey"]) {
    return content::AccessibilityTextStyleUnderlinePredicate;
  } else if ([searchKey isEqualToString:@"AXUnvisitedLinkSearchKey"]) {
    return content::AccessibilityUnvisitedLinkPredicate;
  } else if ([searchKey isEqualToString:@"AXVisitedLinkSearchKey"]) {
    return content::AccessibilityVisitedLinkPredicate;
  }

  return nullptr;
}

// Initialize a OneShotAccessibilityTreeSearch object given the parameters
// passed to AXUIElementCountForSearchPredicate or
// AXUIElementsForSearchPredicate. Return true on success.
bool InitializeAccessibilityTreeSearch(OneShotAccessibilityTreeSearch* search,
                                       id parameter) {
  if (![parameter isKindOfClass:[NSDictionary class]])
    return false;
  NSDictionary* dictionary = parameter;

  id startElementParameter = [dictionary objectForKey:@"AXStartElement"];
  if ([startElementParameter isKindOfClass:[BrowserAccessibilityCocoa class]]) {
    BrowserAccessibilityCocoa* startNodeCocoa =
        (BrowserAccessibilityCocoa*)startElementParameter;
    search->SetStartNode([startNodeCocoa owner]);
  }

  bool immediateDescendantsOnly = false;
  NSNumber* immediateDescendantsOnlyParameter =
      [dictionary objectForKey:@"AXImmediateDescendantsOnly"];
  if ([immediateDescendantsOnlyParameter isKindOfClass:[NSNumber class]])
    immediateDescendantsOnly = [immediateDescendantsOnlyParameter boolValue];

  bool onscreenOnly = false;
  // AXVisibleOnly actually means onscreen objects only -- nothing scrolled off.
  NSNumber* onscreenOnlyParameter = [dictionary objectForKey:@"AXVisibleOnly"];
  if ([onscreenOnlyParameter isKindOfClass:[NSNumber class]])
    onscreenOnly = [onscreenOnlyParameter boolValue];

  content::OneShotAccessibilityTreeSearch::Direction direction =
      content::OneShotAccessibilityTreeSearch::FORWARDS;
  NSString* directionParameter = [dictionary objectForKey:@"AXDirection"];
  if ([directionParameter isKindOfClass:[NSString class]]) {
    if ([directionParameter isEqualToString:@"AXDirectionNext"])
      direction = content::OneShotAccessibilityTreeSearch::FORWARDS;
    else if ([directionParameter isEqualToString:@"AXDirectionPrevious"])
      direction = content::OneShotAccessibilityTreeSearch::BACKWARDS;
  }

  int resultsLimit = kAXResultsLimitNoLimit;
  NSNumber* resultsLimitParameter = [dictionary objectForKey:@"AXResultsLimit"];
  if ([resultsLimitParameter isKindOfClass:[NSNumber class]])
    resultsLimit = [resultsLimitParameter intValue];

  std::string searchText;
  NSString* searchTextParameter = [dictionary objectForKey:@"AXSearchText"];
  if ([searchTextParameter isKindOfClass:[NSString class]])
    searchText = base::SysNSStringToUTF8(searchTextParameter);

  search->SetDirection(direction);
  search->SetImmediateDescendantsOnly(immediateDescendantsOnly);
  search->SetOnscreenOnly(onscreenOnly);
  search->SetSearchText(searchText);

  // Mac uses resultsLimit == -1 for unlimited, that that's
  // the default for OneShotAccessibilityTreeSearch already.
  // Only set the results limit if it's nonnegative.
  if (resultsLimit >= 0)
    search->SetResultLimit(resultsLimit);

  id searchKey = [dictionary objectForKey:@"AXSearchKey"];
  if ([searchKey isKindOfClass:[NSString class]]) {
    AccessibilityMatchPredicate predicate =
        PredicateForSearchKey((NSString*)searchKey);
    if (predicate)
      search->AddPredicate(predicate);
  } else if ([searchKey isKindOfClass:[NSArray class]]) {
    size_t searchKeyCount = static_cast<size_t>([searchKey count]);
    for (size_t i = 0; i < searchKeyCount; ++i) {
      id key = [searchKey objectAtIndex:i];
      if ([key isKindOfClass:[NSString class]]) {
        AccessibilityMatchPredicate predicate =
            PredicateForSearchKey((NSString*)key);
        if (predicate)
          search->AddPredicate(predicate);
      }
    }
  }

  return true;
}

void AppendTextToString(const std::string& extra_text, std::string* string) {
  if (extra_text.empty())
    return;

  if (string->empty()) {
    *string = extra_text;
    return;
  }

  *string += std::string(". ") + extra_text;
}

bool IsSelectedStateRelevant(BrowserAccessibility* item) {
  if (!item->HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    return false;  // Does not have selected state -> not relevant.

  BrowserAccessibility* container = item->PlatformGetSelectionContainer();
  if (!container)
    return false;  // No container -> not relevant.

  if (container->HasState(ax::mojom::State::kMultiselectable))
    return true;  // In a multiselectable -> is relevant.

  // Single selection AND not selected - > is relevant.
  // Single selection containers can explicitly set the focused item as not
  // selected, for example via aria-selectable="false". It's useful for the user
  // to know that it's not selected in this case.
  // Only do this for the focused item -- that is the only item where explicitly
  // setting the item to unselected is relevant, as the focused item is the only
  // item that could have been selected annyway.
  // Therefore, if the user navigates to other items by detaching accessibility
  // focus from the input focus via VO+Shift+F3, those items will not be
  // redundantly reported as not selected.
  return item->manager()->GetFocus() == item &&
         !item->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

}  // namespace

namespace content {

AXTextEdit::AXTextEdit() = default;
AXTextEdit::AXTextEdit(std::u16string inserted_text,
                       std::u16string deleted_text,
                       id edit_text_marker)
    : inserted_text(inserted_text),
      deleted_text(deleted_text),
      edit_text_marker(edit_text_marker, base::scoped_policy::RETAIN) {}
AXTextEdit::AXTextEdit(const AXTextEdit& other) = default;
AXTextEdit::~AXTextEdit() = default;

}  // namespace content

// Not defined in current versions of library, but may be in the future:
#ifndef NSAccessibilityLanguageAttribute
#define NSAccessibilityLanguageAttribute @"AXLanguage"
#endif

bool content::IsNSRange(id value) {
  return [value isKindOfClass:[NSValue class]] &&
         0 == strcmp([value objCType], @encode(NSRange));
}

bool content::IsAXTextMarker(id object) {
  if (object == nil)
    return false;

  AXTextMarkerRef cf_text_marker = static_cast<AXTextMarkerRef>(object);
  DCHECK(cf_text_marker);
  return CFGetTypeID(cf_text_marker) == AXTextMarkerGetTypeID();
}

bool content::IsAXTextMarkerRange(id object) {
  if (object == nil)
    return false;

  AXTextMarkerRangeRef cf_marker_range =
      static_cast<AXTextMarkerRangeRef>(object);
  DCHECK(cf_marker_range);
  return CFGetTypeID(cf_marker_range) == AXTextMarkerRangeGetTypeID();
}

BrowserAccessibility::AXPosition content::AXTextMarkerToAXPosition(
    id text_marker) {
  return CreatePositionFromTextMarker(text_marker);
}

BrowserAccessibility::AXRange content::AXTextMarkerRangeToAXRange(
    id text_marker_range) {
  return CreateRangeFromTextMarkerRange(text_marker_range);
}

id content::AXTextMarkerFrom(const BrowserAccessibilityCocoa* anchor,
                             int offset,
                             ax::mojom::TextAffinity affinity) {
  BrowserAccessibility* anchor_node = [anchor owner];
  BrowserAccessibility::AXPosition position =
      CreateTextPosition(*anchor_node, offset, affinity);
  return CreateTextMarker(std::move(position));
}

id content::AXTextMarkerRangeFrom(id anchor_textmarker, id focus_textmarker) {
  AXTextMarkerRangeRef cf_marker_range = AXTextMarkerRangeCreate(
      kCFAllocatorDefault, static_cast<AXTextMarkerRef>(anchor_textmarker),
      static_cast<AXTextMarkerRef>(focus_textmarker));
  return [static_cast<id>(cf_marker_range) autorelease];
}

@implementation BrowserAccessibilityCocoa

+ (void)initialize {
  const struct {
    NSString* attribute;
    NSString* methodName;
  } attributeToMethodNameContainer[] = {
      {NSAccessibilityBlockQuoteLevelAttribute, @"blockQuoteLevel"},
      {NSAccessibilityChildrenAttribute, @"children"},
      {NSAccessibilityIdentifierChromeAttribute, @"internalId"},
      {NSAccessibilityColumnsAttribute, @"columns"},
      {NSAccessibilityColumnIndexRangeAttribute, @"columnIndexRange"},
      {NSAccessibilityContentsAttribute, @"contents"},
      {NSAccessibilityDescriptionAttribute, @"descriptionForAccessibility"},
      {NSAccessibilityDisclosingAttribute, @"disclosing"},
      {NSAccessibilityDisclosedByRowAttribute, @"disclosedByRow"},
      {NSAccessibilityDisclosureLevelAttribute, @"disclosureLevel"},
      {NSAccessibilityDisclosedRowsAttribute, @"disclosedRows"},
      {NSAccessibilityDropEffectsAttribute, @"dropEffects"},
      {NSAccessibilityDOMClassList, @"domClassList"},
      {NSAccessibilityEditableAncestorAttribute, @"editableAncestor"},
      {NSAccessibilityElementBusyAttribute, @"elementBusy"},
      {NSAccessibilityEnabledAttribute, @"enabled"},
      {NSAccessibilityEndTextMarkerAttribute, @"endTextMarker"},
      {NSAccessibilityExpandedAttribute, @"expanded"},
      {NSAccessibilityFocusableAncestorAttribute, @"focusableAncestor"},
      {NSAccessibilityFocusedAttribute, @"focused"},
      {NSAccessibilityGrabbedAttribute, @"grabbed"},
      {NSAccessibilityHeaderAttribute, @"header"},
      {NSAccessibilityHelpAttribute, @"help"},
      {NSAccessibilityHighestEditableAncestorAttribute,
       @"highestEditableAncestor"},
      {NSAccessibilityIndexAttribute, @"index"},
      {NSAccessibilityInsertionPointLineNumberAttribute,
       @"insertionPointLineNumber"},
      {NSAccessibilityIsMultiSelectableAttribute, @"isMultiSelectable"},
      {NSAccessibilityLanguageAttribute, @"language"},
      {NSAccessibilityLinkedUIElementsAttribute, @"linkedUIElements"},
      {NSAccessibilityLoadingProgressAttribute, @"loadingProgress"},
      {NSAccessibilityKeyShortcutsValueAttribute, @"keyShortcutsValue"},
      {NSAccessibilityMaxValueAttribute, @"maxValue"},
      {NSAccessibilityMinValueAttribute, @"minValue"},
      {NSAccessibilityNumberOfCharactersAttribute, @"numberOfCharacters"},
      {NSAccessibilityOrientationAttribute, @"orientation"},
      {NSAccessibilityOwnsAttribute, @"owns"},
      {NSAccessibilityParentAttribute, @"parent"},
      {NSAccessibilityPositionAttribute, @"position"},
      {NSAccessibilityRoleAttribute, @"role"},
      {NSAccessibilityRowHeaderUIElementsAttribute, @"rowHeaders"},
      {NSAccessibilityRowIndexRangeAttribute, @"rowIndexRange"},
      {NSAccessibilityRowsAttribute, @"rows"},
      // TODO(aboxhall): expose
      // NSAccessibilityServesAsTitleForUIElementsAttribute
      {NSAccessibilityStartTextMarkerAttribute, @"startTextMarker"},
      {NSAccessibilitySelectedAttribute, @"selected"},
      {NSAccessibilitySelectedChildrenAttribute, @"selectedChildren"},
      {NSAccessibilitySelectedTextAttribute, @"selectedText"},
      {NSAccessibilitySelectedTextRangeAttribute, @"selectedTextRange"},
      {NSAccessibilitySelectedTextMarkerRangeAttribute,
       @"selectedTextMarkerRange"},
      {NSAccessibilitySizeAttribute, @"size"},
      {NSAccessibilitySortDirectionAttribute, @"sortDirection"},
      {NSAccessibilitySubroleAttribute, @"subrole"},
      {NSAccessibilityTabsAttribute, @"tabs"},
      {NSAccessibilityTitleAttribute, @"title"},
      {NSAccessibilityTitleUIElementAttribute, @"titleUIElement"},
      {NSAccessibilityTopLevelUIElementAttribute, @"window"},
      {NSAccessibilityValueAttribute, @"value"},
      {NSAccessibilityValueAutofillAvailableAttribute,
       @"valueAutofillAvailable"},
      // Not currently supported by Chrome -- information not stored:
      // {NSAccessibilityValueAutofilledAttribute, @"valueAutofilled"},
      // Not currently supported by Chrome -- mismatch of types supported:
      // {NSAccessibilityValueAutofillTypeAttribute, @"valueAutofillType"},
      {NSAccessibilityValueDescriptionAttribute, @"valueDescription"},
      {NSAccessibilityVisibleCharacterRangeAttribute, @"visibleCharacterRange"},
      {NSAccessibilityVisibleCellsAttribute, @"visibleCells"},
      {NSAccessibilityVisibleChildrenAttribute, @"visibleChildren"},
      {NSAccessibilityVisibleColumnsAttribute, @"visibleColumns"},
      {NSAccessibilityVisibleRowsAttribute, @"visibleRows"},
      {NSAccessibilityVisitedAttribute, @"visited"},
      {NSAccessibilityWindowAttribute, @"window"},
      {@"AXLoaded", @"loaded"},
  };

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  const size_t numAttributes = sizeof(attributeToMethodNameContainer) /
                               sizeof(attributeToMethodNameContainer[0]);
  for (size_t i = 0; i < numAttributes; ++i) {
    [dict setObject:attributeToMethodNameContainer[i].methodName
             forKey:attributeToMethodNameContainer[i].attribute];
  }
  attributeToMethodNameMap = dict;
  dict = nil;
}

- (instancetype)initWithObject:(BrowserAccessibility*)accessibility
              withPlatformNode:(ui::AXPlatformNodeMac*)platform_node {
  if ((self = [super initWithNode:platform_node])) {
    _owner = accessibility;
    _needsToUpdateChildren = true;
    _gettingChildren = false;
  }
  return self;
}

- (void)detach {
  if (!_owner)
    return;

  _owner = nullptr;
  [super detach];
}

- (id)blockQuoteLevel {
  if (![self instanceActive])
    return nil;
  // TODO(accessibility) This is for the number of ancestors that are a
  // <blockquote>, including self, useful for tracking replies to replies etc.
  // in an email.
  int level = 0;
  BrowserAccessibility* ancestor = _owner;
  while (ancestor) {
    if (ancestor->GetRole() == ax::mojom::Role::kBlockquote)
      ++level;
    ancestor = ancestor->PlatformGetParent();
  }
  return @(level);
}

- (NSArray*)AXChildren {
  return [self children];
}

// Returns an array of BrowserAccessibilityCocoa objects, representing the
// accessibility children of this object.
- (NSArray*)children {
  if (![self instanceActive])
    return nil;
  if (_needsToUpdateChildren) {
    base::AutoReset<bool> set_getting_children(&_gettingChildren, true);
    // PlatformChildCount may add extra mac nodes if the node requires them.
    uint32_t childCount = _owner->PlatformChildCount();
    _children.reset([[NSMutableArray alloc] initWithCapacity:childCount]);
    for (auto it = _owner->PlatformChildrenBegin();
         it != _owner->PlatformChildrenEnd(); ++it) {
      BrowserAccessibilityCocoa* child = ToBrowserAccessibilityCocoa(it.get());
      if ([child isIgnored])
        [_children addObjectsFromArray:[child children]];
      else
        [_children addObject:child];
    }

    // Also, add indirect children (if any).
    const std::vector<int32_t>& indirectChildIds = _owner->GetIntListAttribute(
        ax::mojom::IntListAttribute::kIndirectChildIds);
    for (uint32_t i = 0; i < indirectChildIds.size(); ++i) {
      int32_t child_id = indirectChildIds[i];
      BrowserAccessibility* child = _owner->manager()->GetFromID(child_id);

      // This only became necessary as a result of crbug.com/93095. It should be
      // a DCHECK in the future.
      if (child) {
        BrowserAccessibilityCocoa* child_cocoa =
            ToBrowserAccessibilityCocoa(child);
        [_children addObject:child_cocoa];
      }
    }
    _needsToUpdateChildren = false;
  }
  return _children;
}

- (void)childrenChanged {
  // This function may be called in the middle of children() if this node adds
  // extra mac nodes while its children are being requested. If _gettingChildren
  // is true, we don't need to do anything here.
  if (![self instanceActive] || _gettingChildren)
    return;
  _needsToUpdateChildren = true;
  if ([self isIgnored]) {
    BrowserAccessibility* parent = _owner->PlatformGetParent();
    if (parent)
      [ToBrowserAccessibilityCocoa(parent) childrenChanged];
  }
}

- (NSValue*)columnIndexRange {
  if (![self instanceActive])
    return nil;

  absl::optional<int> column = _owner->node()->GetTableCellColIndex();
  absl::optional<int> colspan = _owner->node()->GetTableCellColSpan();
  if (column && colspan)
    return [NSValue valueWithRange:NSMakeRange(*column, *colspan)];
  return nil;
}

- (NSArray*)columns {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  for (BrowserAccessibilityCocoa* child in [self children]) {
    if ([[child role] isEqualToString:NSAccessibilityColumnRole])
      [ret addObject:child];
  }
  return ret;
}

- (BrowserAccessibility*)containingTable {
  BrowserAccessibility* table = _owner;
  while (table && !ui::IsTableLike(table->GetRole())) {
    table = table->PlatformGetParent();
  }
  return table;
}

- (NSString*)AXDescription {
  return [self descriptionForAccessibility];
}

- (NSString*)descriptionForAccessibility {
  if (![self instanceActive])
    return nil;

  // Mac OS X wants static text exposed in AXValue.
  if (ui::IsNameExposedInAXValueForRole([self internalRole]))
    return @"";

  // If we're exposing the title in TitleUIElement, don't also redundantly
  // expose it in AXDescription.
  if ([self shouldExposeTitleUIElement])
    return @"";

  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      _owner->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
  std::string name = _owner->GetName();

  auto status = _owner->GetData().GetImageAnnotationStatus();
  switch (status) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed: {
      std::u16string status_string =
          _owner->GetLocalizedStringForImageAnnotationStatus(status);
      AppendTextToString(base::UTF16ToUTF8(status_string), &name);
      break;
    }

    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      AppendTextToString(_owner->GetStringAttribute(
                             ax::mojom::StringAttribute::kImageAnnotation),
                         &name);
      break;

    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      break;
  }

  if (!name.empty()) {
    // On Mac OS X, the accessible name of an object is exposed as its
    // title if it comes from visible text, and as its description
    // otherwise, but never both.

    // Group, radiogroup etc.
    if ([self shouldExposeNameInDescription]) {
      return base::SysUTF8ToNSString(name);
    } else if (nameFrom == ax::mojom::NameFrom::kCaption ||
               nameFrom == ax::mojom::NameFrom::kContents ||
               nameFrom == ax::mojom::NameFrom::kRelatedElement ||
               nameFrom == ax::mojom::NameFrom::kValue) {
      return @"";
    } else {
      return base::SysUTF8ToNSString(name);
    }
  }

  // Given an image where there's no other title, return the base part
  // of the filename as the description.
  if ([[self role] isEqualToString:NSAccessibilityImageRole]) {
    if ([self titleUIElement])
      return @"";

    std::string url;
    if (_owner->GetStringAttribute(ax::mojom::StringAttribute::kUrl, &url)) {
      // Given a url like http://foo.com/bar/baz.png, just return the
      // base name, e.g., "baz.png".
      size_t leftIndex = url.rfind('/');
      std::string basename =
          leftIndex != std::string::npos ? url.substr(leftIndex) : url;
      return base::SysUTF8ToNSString(basename);
    }
  }

  return @"";
}

- (NSNumber*)disclosing {
  if (![self instanceActive])
    return nil;
  if ([self internalRole] == ax::mojom::Role::kTreeItem) {
    return @(GetState(_owner, ax::mojom::State::kExpanded));
  } else {
    return nil;
  }
}

- (id)disclosedByRow {
  if (![self instanceActive])
    return nil;

  // The row that contains this row.
  // It should be the same as the first parent that is a treeitem.
  return nil;
}

- (NSNumber*)disclosureLevel {
  if (![self instanceActive])
    return nil;
  ax::mojom::Role role = [self internalRole];
  if (role == ax::mojom::Role::kRow || role == ax::mojom::Role::kTreeItem ||
      role == ax::mojom::Role::kHeading) {
    int level =
        _owner->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel);
    // Mac disclosureLevel is 0-based, but web levels are 1-based.
    if (level > 0)
      level--;
    return @(level);
  } else {
    return nil;
  }
}

- (id)disclosedRows {
  if (![self instanceActive])
    return nil;

  // The rows that are considered inside this row.
  return nil;
}

- (NSString*)dropEffects {
  if (![self instanceActive])
    return nil;

  std::string dropEffects;
  if (_owner->GetHtmlAttribute("aria-dropeffect", &dropEffects))
    return base::SysUTF8ToNSString(dropEffects);

  return nil;
}

- (NSArray*)domClassList {
  if (![self instanceActive])
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  std::string classes;
  if (_owner->GetStringAttribute(ax::mojom::StringAttribute::kClassName,
                                 &classes)) {
    std::vector<std::string> split_classes = base::SplitString(
        classes, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& className : split_classes)
      [ret addObject:(base::SysUTF8ToNSString(className))];
  }
  return ret;
}

- (id)editableAncestor {
  if (![self instanceActive])
    return nil;
  const BrowserAccessibility* text_field_ancestor =
      _owner->PlatformGetTextFieldAncestor();
  if (text_field_ancestor)
    return ToBrowserAccessibilityCocoa(text_field_ancestor);
  return nil;
}

- (NSNumber*)elementBusy {
  if (![self instanceActive])
    return nil;
  return @(_owner->GetBoolAttribute(ax::mojom::BoolAttribute::kBusy));
}

- (NSNumber*)enabled {
  if (![self instanceActive])
    return nil;
  return @(_owner->GetData().GetRestriction() !=
           ax::mojom::Restriction::kDisabled);
}

// Returns a text marker that points to the last character in the document that
// can be selected with VoiceOver.
- (id)endTextMarker {
  if (![self instanceActive])
    return nil;
  BrowserAccessibility::AXPosition position = _owner->CreateTextPositionAt(0);
  return CreateTextMarker(position->CreatePositionAtEndOfContent());
}

- (NSNumber*)expanded {
  if (![self instanceActive])
    return nil;
  return @(GetState(_owner, ax::mojom::State::kExpanded));
}

- (id)focusableAncestor {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityCocoa* focusableRoot = self;
  while (![focusableRoot owner]->HasState(ax::mojom::State::kFocusable)) {
    BrowserAccessibilityCocoa* parent = [focusableRoot parent];
    if (!parent || ![parent isKindOfClass:[self class]] ||
        ![parent instanceActive]) {
      return nil;
    }
    focusableRoot = parent;
  }
  return focusableRoot;
}

- (NSNumber*)focused {
  if (![self instanceActive])
    return nil;
  BrowserAccessibilityManager* manager = _owner->manager();
  return @(manager->GetFocus() == _owner);
}

- (NSNumber*)grabbed {
  if (![self instanceActive])
    return nil;
  std::string grabbed;
  if (_owner->GetHtmlAttribute("aria-grabbed", &grabbed) && grabbed == "true")
    return @YES;

  return @NO;
}

- (id)header {
  if (![self instanceActive])
    return nil;
  int headerElementId = -1;
  if (ui::IsTableLike(_owner->GetRole())) {
    // The table header container is always the last child of the table,
    // if it exists. The table header container is a special node in the
    // accessibility tree only used on macOS. It has all of the table
    // headers as its children, even though those cells are also children
    // of rows in the table. Internally this is implemented using
    // AXTableInfo and indirect_child_ids.
    uint32_t childCount = _owner->PlatformChildCount();
    if (childCount > 0) {
      BrowserAccessibility* tableHeader = _owner->PlatformGetLastChild();
      if (tableHeader->GetRole() == ax::mojom::Role::kTableHeaderContainer)
        return ToBrowserAccessibilityCocoa(tableHeader);
    }
  } else if ([self internalRole] == ax::mojom::Role::kColumn) {
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTableColumnHeaderId,
                            &headerElementId);
  } else if ([self internalRole] == ax::mojom::Role::kRow) {
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTableRowHeaderId,
                            &headerElementId);
  }

  if (headerElementId > 0) {
    BrowserAccessibility* headerObject =
        _owner->manager()->GetFromID(headerElementId);
    if (headerObject)
      return ToBrowserAccessibilityCocoa(headerObject);
  }
  return nil;
}

- (NSString*)help {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(_owner,
                                    ax::mojom::StringAttribute::kDescription);
}

- (id)highestEditableAncestor {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityCocoa* highestEditableAncestor = [self editableAncestor];
  while (highestEditableAncestor) {
    BrowserAccessibilityCocoa* ancestorParent =
        [highestEditableAncestor parent];
    if (!ancestorParent || ![ancestorParent isKindOfClass:[self class]]) {
      break;
    }
    BrowserAccessibilityCocoa* higherAncestor =
        [ancestorParent editableAncestor];
    if (!higherAncestor)
      break;
    highestEditableAncestor = higherAncestor;
  }
  return highestEditableAncestor;
}

- (NSNumber*)index {
  if (![self instanceActive])
    return nil;

  if ([self internalRole] == ax::mojom::Role::kTreeItem) {
    return [self treeItemRowIndex];
  } else if ([self internalRole] == ax::mojom::Role::kColumn) {
    DCHECK(_owner->node());
    absl::optional<int> col_index = *_owner->node()->GetTableColColIndex();
    if (col_index)
      return @(*col_index);
  } else if ([self internalRole] == ax::mojom::Role::kRow) {
    DCHECK(_owner->node());
    absl::optional<int> row_index = _owner->node()->GetTableRowRowIndex();
    if (row_index)
      return @(*row_index);
  }

  return nil;
}

- (NSNumber*)treeItemRowIndex {
  if (![self instanceActive])
    return nil;

  DCHECK([self internalRole] == ax::mojom::Role::kTreeItem);
  DCHECK([[self role] isEqualToString:NSAccessibilityRowRole]);

  // First find an ancestor that establishes this tree or treegrid. We
  // will search in this ancestor to calculate our row index.
  BrowserAccessibility* container = [self owner]->PlatformGetParent();
  while (container && container->GetRole() != ax::mojom::Role::kTree &&
         container->GetRole() != ax::mojom::Role::kTreeGrid) {
    container = container->PlatformGetParent();
  }
  if (!container)
    return nil;

  const BrowserAccessibilityCocoa* cocoaContainer =
      ToBrowserAccessibilityCocoa(container);
  int currentIndex = 0;
  if ([cocoaContainer findRowIndex:self withCurrentIndex:&currentIndex]) {
    return @(currentIndex);
  }

  return nil;
}

- (bool)findRowIndex:(BrowserAccessibilityCocoa*)toFind
    withCurrentIndex:(int*)currentIndex {
  if (![self instanceActive])
    return false;

  DCHECK([[toFind role] isEqualToString:NSAccessibilityRowRole]);
  for (BrowserAccessibilityCocoa* childToCheck in [self children]) {
    if ([toFind isEqual:childToCheck]) {
      return true;
    }

    if ([[childToCheck role] isEqualToString:NSAccessibilityRowRole]) {
      ++(*currentIndex);
    }

    if ([childToCheck findRowIndex:toFind withCurrentIndex:currentIndex]) {
      return true;
    }
  }

  return false;
}

- (NSNumber*)AXInsertionPointLineNumber {
  return [self insertionPointLineNumber];
}

- (NSNumber*)insertionPointLineNumber {
  if (![self instanceActive])
    return nil;
  if (!_owner->HasVisibleCaretOrSelection())
    return nil;

  const BrowserAccessibility::AXRange range = GetSelectedRange(*_owner);
  // If the selection is not collapsed, then there is no visible caret.
  if (!range.IsCollapsed())
    return nil;

  // "ax::mojom::MoveDirection" is only relevant on platforms that use object
  // replacement characters in the accessibility tree. Mac is not one of them.
  const BrowserAccessibility::AXPosition caretPosition =
      range.focus()->LowestCommonAncestor(*_owner->CreateTextPositionAt(0),
                                          ax::mojom::MoveDirection::kForward);
  DCHECK(!caretPosition->IsNullPosition())
      << "Calling HasVisibleCaretOrSelection() should have ensured that there "
         "is a valid selection focus inside the current object.";
  const std::vector<int> lineStarts =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
  auto iterator =
      std::lower_bound(lineStarts.begin(), lineStarts.end(),
                       caretPosition->AsTextPosition()->text_offset());
  return @(std::distance(lineStarts.begin(), iterator));
}

// Returns whether or not this node should be ignored in the
// accessibility tree.
- (BOOL)isIgnored {
  if (![self instanceActive])
    return YES;

  return [[self role] isEqualToString:NSAccessibilityUnknownRole] ||
         _owner->IsInvisibleOrIgnored();
}

- (NSNumber*)isMultiSelectable {
  if (![self instanceActive])
    return nil;
  return @(GetState(_owner, ax::mojom::State::kMultiselectable));
}

- (NSString*)language {
  if (![self instanceActive])
    return nil;
  ui::AXNode* node = _owner->node();
  DCHECK(node);
  return base::SysUTF8ToNSString(node->GetLanguage());
}

// private
- (void)addLinkedUIElementsFromAttribute:(ax::mojom::IntListAttribute)attribute
                                   addTo:(NSMutableArray*)outArray {
  const std::vector<int32_t>& attributeValues =
      _owner->GetIntListAttribute(attribute);
  for (size_t i = 0; i < attributeValues.size(); ++i) {
    BrowserAccessibility* element =
        _owner->manager()->GetFromID(attributeValues[i]);
    if (element)
      [outArray addObject:ToBrowserAccessibilityCocoa(element)];
  }
}

// private
- (NSArray*)linkedUIElements {
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [self
      addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::kControlsIds
                                 addTo:ret];
  [self addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::kFlowtoIds
                                   addTo:ret];

  int target_id;
  if (_owner->GetIntAttribute(ax::mojom::IntAttribute::kInPageLinkTargetId,
                              &target_id)) {
    BrowserAccessibility* target =
        _owner->manager()->GetFromID(static_cast<int32_t>(target_id));
    if (target)
      [ret addObject:ToBrowserAccessibilityCocoa(target)];
  }

  [self addLinkedUIElementsFromAttribute:ax::mojom::IntListAttribute::
                                             kRadioGroupIds
                                   addTo:ret];
  return ret;
}

- (NSNumber*)loaded {
  if (![self instanceActive])
    return nil;
  return @YES;
}

- (NSNumber*)loadingProgress {
  if (![self instanceActive])
    return nil;
  BrowserAccessibilityManager* manager = _owner->manager();
  float floatValue = manager->GetTreeData().loading_progress;
  return @(floatValue);
}

- (NSString*)keyShortcutsValue {
  if (![self instanceActive])
    return nil;
  return NSStringForStringAttribute(_owner,
                                    ax::mojom::StringAttribute::kKeyShortcuts);
}

- (NSNumber*)maxValue {
  if (![self instanceActive])
    return nil;

  if (!_owner->HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange))
    return @0;  // Indeterminate value exposes AXMinValue/AXMaxValue of 0.

  float floatValue =
      _owner->GetFloatAttribute(ax::mojom::FloatAttribute::kMaxValueForRange);
  return @(floatValue);
}

- (NSNumber*)minValue {
  if (![self instanceActive])
    return nil;

  if (!_owner->HasFloatAttribute(ax::mojom::FloatAttribute::kValueForRange))
    return @0;  // Indeterminate value exposes AXMinValue/AXMaxValue of 0.

  float floatValue =
      _owner->GetFloatAttribute(ax::mojom::FloatAttribute::kMinValueForRange);
  return @(floatValue);
}

- (NSString*)orientation {
  if (![self instanceActive])
    return nil;
  if (GetState(_owner, ax::mojom::State::kVertical))
    return NSAccessibilityVerticalOrientationValue;
  else if (GetState(_owner, ax::mojom::State::kHorizontal))
    return NSAccessibilityHorizontalOrientationValue;

  return @"";
}

- (id)owns {
  if (![self instanceActive])
    return nil;

  BrowserAccessibility* activeDescendant =
      _owner->manager()->GetActiveDescendant(_owner);
  if (!activeDescendant)
    return nil;

  BrowserAccessibility* container = activeDescendant->PlatformGetParent();
  while (container &&
         !ui::IsContainerWithSelectableChildren(container->GetRole())) {
    container = container->PlatformGetParent();
  }
  if (!container)
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  [ret addObject:ToBrowserAccessibilityCocoa(container)];
  return ret;
}

- (NSNumber*)AXNumberOfCharacters {
  return [self numberOfCharacters];
}

- (NSNumber*)numberOfCharacters {
  if ([self instanceActive] && _owner->IsTextField())
    return @(static_cast<int>(_owner->GetValueForControl().size()));
  return nil;
}

// The origin of this accessibility object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
- (NSPoint)origin {
  if (![self instanceActive])
    return NSMakePoint(0, 0);
  gfx::Rect bounds = _owner->GetClippedRootFrameBoundsRect();
  return NSMakePoint(bounds.x(), bounds.y());
}

- (id)parent {
  if (![self instanceActive])
    return nil;
  // A nil parent means we're the root.
  if (_owner->PlatformGetParent()) {
    return NSAccessibilityUnignoredAncestor(
        ToBrowserAccessibilityCocoa(_owner->PlatformGetParent()));
  } else {
    // Hook back up to RenderWidgetHostViewCocoa.
    BrowserAccessibilityManagerMac* manager =
        _owner->manager()->GetRootManager()->ToBrowserAccessibilityManagerMac();
    if (manager)
      return manager->GetParentView();
    return nil;
  }
}

- (NSValue*)position {
  if (![self instanceActive])
    return nil;
  NSPoint origin = [self origin];
  NSSize size = [[self size] sizeValue];
  NSPoint pointInScreen =
      [self rectInScreen:gfx::Rect(gfx::Point(origin), gfx::Size(size))].origin;
  return [NSValue valueWithPoint:pointInScreen];
}

// Returns an enum indicating the role from owner_.
// internal
- (ax::mojom::Role)internalRole {
  if ([self instanceActive])
    return static_cast<ax::mojom::Role>(_owner->GetRole());
  return ax::mojom::Role::kNone;
}

- (BOOL)shouldExposeNameInDescription {
  // Image annotations are not visible text, so they should be exposed
  // as a description and not a title.
  switch (_owner->GetData().GetImageAnnotationStatus()) {
    case ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kAnnotationPending:
    case ax::mojom::ImageAnnotationStatus::kAnnotationEmpty:
    case ax::mojom::ImageAnnotationStatus::kAnnotationAdult:
    case ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed:
    case ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded:
      return true;

    case ax::mojom::ImageAnnotationStatus::kNone:
    case ax::mojom::ImageAnnotationStatus::kWillNotAnnotateDueToScheme:
    case ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation:
    case ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation:
      break;
  }

  // VoiceOver computes the wrong description for a link.
  if (ui::IsLink(_owner->GetRole()))
    return true;

  // VoiceOver will not read the label of these roles unless it is
  // exposed in the description instead of the title.
  switch (_owner->GetRole()) {
    case ax::mojom::Role::kGenericContainer:
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kRadioGroup:
      return true;
    default:
      return false;
  }
}

// Returns true if this object should expose its accessible name using
// AXTitleUIElement rather than AXTitle or AXDescription. We only do
// this if it's a control, if there's a single label, and the label has
// nonempty text.
// internal
- (BOOL)shouldExposeTitleUIElement {
  // VoiceOver ignores TitleUIElement if the element isn't a control.
  if (!ui::IsControl(_owner->GetRole()))
    return false;

  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      _owner->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
  if (nameFrom != ax::mojom::NameFrom::kCaption &&
      nameFrom != ax::mojom::NameFrom::kRelatedElement)
    return false;

  std::vector<int32_t> labelledby_ids =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds);
  if (labelledby_ids.size() != 1)
    return false;

  BrowserAccessibility* label = _owner->manager()->GetFromID(labelledby_ids[0]);
  if (!label)
    return false;

  std::string labelName = label->GetName();
  return !labelName.empty();
}

// internal
- (content::BrowserAccessibilityDelegate*)delegate {
  return [self instanceActive] ? _owner->manager()->delegate() : nil;
}

- (content::BrowserAccessibility*)owner {
  return _owner;
}

// Assumes that there is at most one insertion, deletion or replacement at once.
// TODO(nektar): Merge this method with
// |BrowserAccessibilityAndroid::CommonEndLengths|.
- (content::AXTextEdit)computeTextEdit {
  if (!_owner->IsTextField())
    return content::AXTextEdit();

  // Starting from macOS 10.11, if the user has edited some text we need to
  // dispatch the actual text that changed on the value changed notification.
  // We run this code on all macOS versions to get the highest test coverage.
  std::u16string oldValue = _oldValue;
  std::u16string newValue = _owner->GetValueForControl();
  _oldValue = newValue;
  if (oldValue.empty() && newValue.empty())
    return content::AXTextEdit();

  size_t i;
  size_t j;
  // Sometimes Blink doesn't use the same UTF16 characters to represent
  // whitespace.
  for (i = 0;
       i < oldValue.length() && i < newValue.length() &&
       (oldValue[i] == newValue[i] || (base::IsUnicodeWhitespace(oldValue[i]) &&
                                       base::IsUnicodeWhitespace(newValue[i])));
       ++i) {
  }
  for (j = 0;
       (i + j) < oldValue.length() && (i + j) < newValue.length() &&
       (oldValue[oldValue.length() - j - 1] ==
            newValue[newValue.length() - j - 1] ||
        (base::IsUnicodeWhitespace(oldValue[oldValue.length() - j - 1]) &&
         base::IsUnicodeWhitespace(newValue[newValue.length() - j - 1])));
       ++j) {
  }
  DCHECK_LE(i + j, oldValue.length());
  DCHECK_LE(i + j, newValue.length());

  std::u16string deletedText = oldValue.substr(i, oldValue.length() - i - j);
  std::u16string insertedText = newValue.substr(i, newValue.length() - i - j);

  // Heuristic for editable combobox. If more than 1 character is inserted or
  // deleted, and the caret is at the end of the field, assume the entire text
  // field changed.
  // TODO(nektar) Remove this once editing intents are implemented,
  // and the actual inserted and deleted text is passed over from Blink.
  if ([self internalRole] == ax::mojom::Role::kTextFieldWithComboBox &&
      (deletedText.length() > 1 || insertedText.length() > 1)) {
    int sel_start, sel_end;
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTextSelStart, &sel_start);
    _owner->GetIntAttribute(ax::mojom::IntAttribute::kTextSelEnd, &sel_end);
    if (static_cast<size_t>(sel_start) == newValue.length() &&
        static_cast<size_t>(sel_end) == newValue.length()) {
      // Don't include oldValue as it would be announced -- very confusing.
      return content::AXTextEdit(newValue, std::u16string(), nil);
    }
  }
  return content::AXTextEdit(insertedText, deletedText,
                             CreateTextMarker(_owner->CreateTextPositionAt(i)));
}

// internal
- (NSRect)rectInScreen:(gfx::Rect)layout_rect {
  if (![self instanceActive])
    return NSZeroRect;

  // Convert to DIPs if UseZoomForDSF is enabled.
  auto rect =
      IsUseZoomForDSFEnabled()
          ? ScaleToRoundedRect(layout_rect,
                               1.f / _owner->manager()->device_scale_factor())
          : layout_rect;

  // Get the delegate for the topmost BrowserAccessibilityManager, because
  // that's the only one that can convert points to their origin in the screen.
  BrowserAccessibilityDelegate* delegate =
      _owner->manager()->GetDelegateFromRootManager();
  if (delegate) {
    return gfx::ScreenRectToNSRect(
        rect + delegate->AccessibilityGetViewBounds().OffsetFromOrigin());
  } else {
    return NSZeroRect;
  }
}

// Returns a string indicating the NSAccessibility role of this object.
- (NSString*)AXRole {
  return [self role];
}
- (NSString*)role {
  content::BrowserAccessibilityStateImpl::GetInstance()
      ->OnAccessibilityApiUsage();

  if (![self instanceActive]) {
    TRACE_EVENT0("accessibility", "BrowserAccessibilityCocoa::role nil");
    return nil;
  }

  NSString* cocoa_role = nil;
  ax::mojom::Role role = [self internalRole];
  if (role == ax::mojom::Role::kCanvas &&
      _owner->GetBoolAttribute(ax::mojom::BoolAttribute::kCanvasHasFallback)) {
    cocoa_role = NSAccessibilityGroupRole;
  } else if (_owner->IsTextField() &&
             _owner->HasState(ax::mojom::State::kMultiline)) {
    cocoa_role = NSAccessibilityTextAreaRole;
  } else if (role == ax::mojom::Role::kImage && _owner->GetChildCount()) {
    // An image map is an image with children, and exposed on Mac as a group.
    cocoa_role = NSAccessibilityGroupRole;
  } else if (role == ax::mojom::Role::kImage &&
             _owner->HasExplicitlyEmptyName()) {
    cocoa_role = NSAccessibilityUnknownRole;
  } else if (_owner->IsWebAreaForPresentationalIframe()) {
    cocoa_role = NSAccessibilityGroupRole;
  } else {
    cocoa_role = [AXPlatformNodeCocoa nativeRoleFromAXRole:role];
  }

  TRACE_EVENT1("accessibility", "BrowserAccessibilityCocoa::role",
               "role=", base::SysNSStringToUTF8(cocoa_role));
  return cocoa_role;
}

- (NSArray*)rowHeaders {
  if (![self instanceActive])
    return nil;

  bool is_cell_or_table_header = ui::IsCellOrTableHeader(_owner->GetRole());
  bool is_table_like = ui::IsTableLike(_owner->GetRole());
  if (!is_table_like && !is_cell_or_table_header)
    return nil;
  BrowserAccessibility* table = [self containingTable];
  if (!table)
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  if (is_table_like) {
    // If this is a table, return all row headers.
    std::set<int32_t> headerIds;
    for (int i = 0; i < *table->GetTableRowCount(); i++) {
      std::vector<int32_t> rowHeaderIds = table->GetRowHeaderNodeIds(i);
      for (int32_t id : rowHeaderIds)
        headerIds.insert(id);
    }
    for (int32_t id : headerIds) {
      BrowserAccessibility* cell = _owner->manager()->GetFromID(id);
      if (cell)
        [ret addObject:ToBrowserAccessibilityCocoa(cell)];
    }
  } else {
    // Otherwise this is a cell, return the row headers for this cell.
    for (int32_t id : _owner->node()->GetTableCellRowHeaderNodeIds()) {
      BrowserAccessibility* cell = _owner->manager()->GetFromID(id);
      if (cell)
        [ret addObject:ToBrowserAccessibilityCocoa(cell)];
    }
  }

  return [ret count] ? ret : nil;
}

- (NSValue*)rowIndexRange {
  if (![self instanceActive])
    return nil;

  absl::optional<int> row = _owner->node()->GetTableCellRowIndex();
  absl::optional<int> rowspan = _owner->node()->GetTableCellRowSpan();
  if (row && rowspan)
    return [NSValue valueWithRange:NSMakeRange(*row, *rowspan)];
  return nil;
}

- (NSArray*)rows {
  if (![self instanceActive])
    return nil;
  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];

  std::vector<int32_t> node_id_list;
  if (_owner->GetRole() == ax::mojom::Role::kTree)
    [self getTreeItemDescendantNodeIds:&node_id_list];
  else if (ui::IsTableLike(_owner->GetRole()))
    node_id_list = _owner->node()->GetTableRowNodeIds();
  // Rows attribute for a column is the list of all the elements in that column
  // at each row.
  else if ([self internalRole] == ax::mojom::Role::kColumn)
    node_id_list = _owner->GetIntListAttribute(
        ax::mojom::IntListAttribute::kIndirectChildIds);

  for (int32_t node_id : node_id_list) {
    BrowserAccessibility* rowElement = _owner->manager()->GetFromID(node_id);
    if (rowElement)
      [ret addObject:ToBrowserAccessibilityCocoa(rowElement)];
  }

  return ret;
}

- (NSNumber*)selected {
  if (![self instanceActive])
    return nil;
  return @(_owner->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
}

- (NSArray*)selectedChildren {
  if (![self instanceActive])
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  BrowserAccessibility* focusedChild = _owner->manager()->GetFocus();

  // "IsDescendantOf" also returns true when the two objects are equivalent.
  if (focusedChild && focusedChild != _owner &&
      focusedChild->IsDescendantOf(_owner)) {
    // If this container is not multi-selectable, try to skip iterating over the
    // children because there could only be at most one selected child. The
    // selected child should also be equivalent to the focused child, because
    // selection is tethered to the focus.
    if (!GetState(_owner, ax::mojom::State::kMultiselectable) &&
        focusedChild->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)) {
      [ret addObject:ToBrowserAccessibilityCocoa(focusedChild)];
      return ret;
    }

    // If this container is multi-selectable and the focused child is selected,
    // add the focused child in the list of selected children first, because
    // this is how VoiceOver determines where to draw the focus ring around the
    // active item.
    if (focusedChild->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected))
      [ret addObject:ToBrowserAccessibilityCocoa(focusedChild)];
  }

  // If this container is multi-selectable, we need to return any additional
  // children (other than the focused child) with the "selected" state. If this
  // container is not multi-selectable, but none of its children have the focus,
  // we need to return all its children with the "selected" state.
  for (auto it = _owner->PlatformChildrenBegin();
       it != _owner->PlatformChildrenEnd(); ++it) {
    BrowserAccessibility* child = it.get();
    if (child->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
        child != focusedChild) {
      [ret addObject:ToBrowserAccessibilityCocoa(child)];
    }
  }

  return ret;
}

- (NSString*)AXSelectedText {
  return [self selectedText];
}

- (NSString*)selectedText {
  if (![self instanceActive])
    return nil;
  if (!_owner->HasVisibleCaretOrSelection())
    return nil;

  const BrowserAccessibility::AXRange range = GetSelectedRange(*_owner);
  if (range.IsNull())
    return nil;
  return base::SysUTF16ToNSString(range.GetText());
}

- (NSValue*)AXSelectedTextRange {
  return [self selectedTextRange];
}

// Returns range of text under the current object that is selected.
//
// Example, caret at offset 5:
// NSRange:  “pos=5 len=0”
- (NSValue*)selectedTextRange {
  if (![self instanceActive])
    return nil;
  if (!_owner->HasVisibleCaretOrSelection())
    return nil;

  const BrowserAccessibility::AXRange range =
      GetSelectedRange(*_owner).AsForwardRange();
  if (range.IsNull())
    return nil;

  // "ax::mojom::MoveDirection" is only relevant on platforms that use object
  // replacement characters in the accessibility tree. Mac is not one of them.
  const BrowserAccessibility::AXPosition startPosition =
      range.anchor()->LowestCommonAncestor(*_owner->CreateTextPositionAt(0),
                                           ax::mojom::MoveDirection::kForward);
  DCHECK(!startPosition->IsNullPosition())
      << "Calling HasVisibleCaretOrSelection() should have ensured that there "
         "is a valid selection anchor inside the current object.";
  int selStart = startPosition->AsTextPosition()->text_offset();
  DCHECK_GE(selStart, 0);
  int selLength = range.GetText().length();
  return [NSValue valueWithRange:NSMakeRange(selStart, selLength)];
}

- (id)selectedTextMarkerRange {
  if (![self instanceActive])
    return nil;
  BrowserAccessibility::AXRange ax_range = GetSelectedRange(*_owner);
  if (ax_range.IsNull())
    return nil;

  // Voiceover expects this range to be backwards in order to read the selected
  // words correctly.
  return CreateTextMarkerRange(ax_range.AsBackwardRange());
}

- (NSValue*)size {
  if (![self instanceActive])
    return nil;
  gfx::Rect bounds = _owner->GetClippedRootFrameBoundsRect();
  return [NSValue valueWithSize:NSMakeSize(bounds.width(), bounds.height())];
}

- (NSString*)sortDirection {
  if (![self instanceActive])
    return nil;
  int sortDirection;
  if (!_owner->GetIntAttribute(ax::mojom::IntAttribute::kSortDirection,
                               &sortDirection))
    return nil;

  switch (static_cast<ax::mojom::SortDirection>(sortDirection)) {
    case ax::mojom::SortDirection::kUnsorted:
      return nil;
    case ax::mojom::SortDirection::kAscending:
      return NSAccessibilityAscendingSortDirectionValue;
    case ax::mojom::SortDirection::kDescending:
      return NSAccessibilityDescendingSortDirectionValue;
    case ax::mojom::SortDirection::kOther:
      return NSAccessibilityUnknownSortDirectionValue;
    default:
      NOTREACHED();
  }

  return nil;
}

// Returns a text marker that points to the first character in the document that
// can be selected with VoiceOver.
- (id)startTextMarker {
  if (![self instanceActive])
    return nil;
  BrowserAccessibility::AXPosition position = _owner->CreateTextPositionAt(0);
  return CreateTextMarker(position->CreatePositionAtStartOfContent());
}

- (NSString*)AXSubrole {
  return [self subrole];
}

// Returns a subrole based upon the role.
- (NSString*)subrole {
  if (![self instanceActive])
    return nil;

  if (_owner->IsAtomicTextField() &&
      GetState(_owner, ax::mojom::State::kProtected)) {
    return NSAccessibilitySecureTextFieldSubrole;
  }

  if ([self internalRole] == ax::mojom::Role::kDescriptionList)
    return NSAccessibilityDefinitionListSubrole;

  if ([self internalRole] == ax::mojom::Role::kDirectory ||
      [self internalRole] == ax::mojom::Role::kList) {
    return NSAccessibilityContentListSubrole;
  }

  return [AXPlatformNodeCocoa nativeSubroleFromAXRole:[self internalRole]];
}

// Returns all tabs in this subtree.
- (NSArray*)tabs {
  if (![self instanceActive])
    return nil;
  NSMutableArray* tabSubtree = [[[NSMutableArray alloc] init] autorelease];

  if ([self internalRole] == ax::mojom::Role::kTab)
    [tabSubtree addObject:self];

  for (uint i = 0; i < [[self children] count]; ++i) {
    NSArray* tabChildren = [[[self children] objectAtIndex:i] tabs];
    if ([tabChildren count] > 0)
      [tabSubtree addObjectsFromArray:tabChildren];
  }

  return tabSubtree;
}

- (NSString*)AXTitle {
  return [self title];
}
- (NSString*)title {
  if (![self instanceActive])
    return nil;
  // Mac OS X wants static text exposed in AXValue.
  if (ui::IsNameExposedInAXValueForRole([self internalRole]))
    return @"";

  if ([self shouldExposeNameInDescription])
    return @"";

  // If we're exposing the title in TitleUIElement, don't also redundantly
  // expose it in AXDescription.
  if ([self shouldExposeTitleUIElement])
    return @"";

  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      _owner->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));

  // On Mac OS X, cell titles are "" if it it came from content.
  NSString* role = [self role];
  if ([role isEqualToString:NSAccessibilityCellRole] &&
      nameFrom == ax::mojom::NameFrom::kContents)
    return @"";

  // On Mac OS X, the accessible name of an object is exposed as its
  // title if it comes from visible text, and as its description
  // otherwise, but never both.
  if (nameFrom == ax::mojom::NameFrom::kCaption ||
      nameFrom == ax::mojom::NameFrom::kContents ||
      nameFrom == ax::mojom::NameFrom::kRelatedElement ||
      nameFrom == ax::mojom::NameFrom::kValue) {
    return NSStringForStringAttribute(_owner,
                                      ax::mojom::StringAttribute::kName);
  }

  return @"";
}

- (id)titleUIElement {
  if (![self instanceActive])
    return nil;
  if (![self shouldExposeTitleUIElement])
    return nil;

  std::vector<int32_t> labelledby_ids =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds);
  ax::mojom::NameFrom nameFrom = static_cast<ax::mojom::NameFrom>(
      _owner->GetIntAttribute(ax::mojom::IntAttribute::kNameFrom));
  if ((nameFrom == ax::mojom::NameFrom::kCaption ||
       nameFrom == ax::mojom::NameFrom::kRelatedElement) &&
      labelledby_ids.size() == 1) {
    BrowserAccessibility* titleElement =
        _owner->manager()->GetFromID(labelledby_ids[0]);
    if (titleElement)
      return ToBrowserAccessibilityCocoa(titleElement);
  }

  return nil;
}

- (id)AXValue {
  return [self value];
}
- (id)value {
  if (![self instanceActive])
    return nil;

  if (ui::IsNameExposedInAXValueForRole([self internalRole])) {
    std::u16string name = _owner->GetTextContentUTF16();
    // Leaf node with aria-label will have empty text content.
    // e.g. <div role="option" aria-label="label">content</div>
    // So we use its computed name for AXValue.
    if (name.empty())
      name = _owner->GetNameAsString16();
    if (!IsSelectedStateRelevant(_owner)) {
      return base::SysUTF16ToNSString(name);
    }

    // Append the selection state as a string, because VoiceOver will not
    // automatically report selection state when an individual item is focused.
    bool is_selected =
        _owner->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
    int msg_id =
        is_selected ? IDS_AX_OBJECT_SELECTED : IDS_AX_OBJECT_NOT_SELECTED;
    ContentClient* content_client = content::GetContentClient();
    std::u16string name_with_selection = base::ReplaceStringPlaceholders(
        content_client->GetLocalizedString(msg_id), {name}, nullptr);

    return base::SysUTF16ToNSString(name_with_selection);
  }

  NSString* role = [self role];
  if ([role isEqualToString:@"AXHeading"]) {
    int level = 0;
    if (_owner->GetIntAttribute(ax::mojom::IntAttribute::kHierarchicalLevel,
                                &level)) {
      return @(level);
    }
  } else if ([role isEqualToString:NSAccessibilityButtonRole]) {
    // AXValue does not make sense for pure buttons.
    return @"";
  } else if ([self isCheckable]) {
    int value;
    const auto checkedState = _owner->GetData().GetCheckedState();
    switch (checkedState) {
      case ax::mojom::CheckedState::kTrue:
        value = 1;
        break;
      case ax::mojom::CheckedState::kMixed:
        value = 2;
        break;
      default:
        value = _owner->GetBoolAttribute(ax::mojom::BoolAttribute::kSelected)
                    ? 1
                    : 0;
        break;
    }
    return @(value);
  } else if (_owner->GetData().IsRangeValueSupported()) {
    // Objects that support range values include progress bars, sliders, and
    // steppers. Only the native value or aria-valuenow should be exposed, not
    // the aria-valuetext. Aria-valuetext is exposed via
    // "accessibilityValueDescription".
    float floatValue;
    if (_owner->GetFloatAttribute(ax::mojom::FloatAttribute::kValueForRange,
                                  &floatValue)) {
      return @(floatValue);
    }
  } else if ([role isEqualToString:NSAccessibilityColorWellRole]) {
    unsigned int color = static_cast<unsigned int>(
        _owner->GetIntAttribute(ax::mojom::IntAttribute::kColorValue));
    unsigned int red = SkColorGetR(color);
    unsigned int green = SkColorGetG(color);
    unsigned int blue = SkColorGetB(color);
    // This string matches the one returned by a native Mac color well.
    return [NSString stringWithFormat:@"rgb %7.5f %7.5f %7.5f 1", red / 255.,
                                      green / 255., blue / 255.];
  }

  return base::SysUTF16ToNSString(_owner->GetValueForControl());
}

- (NSNumber*)valueAutofillAvailable {
  if (![self instanceActive])
    return nil;
  return _owner->HasState(ax::mojom::State::kAutofillAvailable) ? @YES : @NO;
}

// Not currently supported, as Chrome does not store whether an autofill
// occurred. We could have autofill fire an event, however, and set an
// "is_autofilled" flag until the next edit. - (NSNumber*)valueAutofilled {
//  return @NO;
// }

// Not currently supported, as Chrome's autofill types aren't like Safari's.
// - (NSString*)valueAutofillType {
//  return @"none";
//}

- (NSString*)valueDescription {
  if (![self instanceActive] || !_owner->GetData().IsRangeValueSupported())
    return nil;

  // This method is only for exposing aria-valuetext to VoiceOver if present.
  // Blink places the value of aria-valuetext in
  // ax::mojom::StringAttribute::kValue for objects that support range values,
  // i.e., progress bars, sliders and steppers.
  return base::SysUTF8ToNSString(
      _owner->GetStringAttribute(ax::mojom::StringAttribute::kValue));
}

- (NSValue*)AXVisibleCharacterRange {
  return [self visibleCharacterRange];
}

- (NSValue*)visibleCharacterRange {
  if ([self instanceActive] && _owner->IsTextField() &&
      !_owner->IsPasswordField()) {
    return [NSValue
        valueWithRange:NSMakeRange(0,
                                   static_cast<int>(
                                       _owner->GetValueForControl().size()))];
  }
  return nil;
}

- (NSArray*)visibleCells {
  if (![self instanceActive])
    return nil;

  NSMutableArray* ret = [[[NSMutableArray alloc] init] autorelease];
  for (int32_t id : _owner->node()->GetTableUniqueCellIds()) {
    BrowserAccessibility* cell = _owner->manager()->GetFromID(id);
    if (cell)
      [ret addObject:ToBrowserAccessibilityCocoa(cell)];
  }
  return ret;
}

- (NSArray*)visibleChildren {
  if (![self instanceActive])
    return nil;
  return [self children];
}

- (NSArray*)visibleColumns {
  if (![self instanceActive])
    return nil;
  return [self columns];
}

- (NSArray*)visibleRows {
  if (![self instanceActive])
    return nil;
  return [self rows];
}

- (NSNumber*)visited {
  if (![self instanceActive])
    return nil;
  return @(GetState(_owner, ax::mojom::State::kVisited));
}

- (id)window {
  if (![self instanceActive])
    return nil;

  BrowserAccessibilityManagerMac* manager =
      _owner->manager()->GetRootManager()->ToBrowserAccessibilityManagerMac();
  if (!manager || !manager->GetParentView())
    return nil;

  return manager->GetWindow();
}

- (void)getTreeItemDescendantNodeIds:(std::vector<int32_t>*)tree_item_ids {
  for (auto it = _owner->PlatformChildrenBegin();
       it != _owner->PlatformChildrenEnd(); ++it) {
    const BrowserAccessibilityCocoa* child =
        ToBrowserAccessibilityCocoa(it.get());

    if ([child internalRole] == ax::mojom::Role::kTreeItem) {
      tree_item_ids->push_back([child hash]);
    }
    [child getTreeItemDescendantNodeIds:tree_item_ids];
  }
}

- (NSString*)methodNameForAttribute:(NSString*)attribute {
  return [attributeToMethodNameMap objectForKey:attribute];
}

- (NSString*)valueForRange:(NSRange)range {
  if (![self instanceActive])
    return nil;

  std::u16string textContent = _owner->GetTextContentUTF16();
  if (NSMaxRange(range) > textContent.length())
    return nil;

  return base::SysUTF16ToNSString(
      textContent.substr(range.location, range.length));
}

// Retrieves the text inside this object and decorates it with attributes
// indicating specific ranges of interest within the text, e.g. the location of
// misspellings.
- (NSAttributedString*)attributedValueForRange:(NSRange)range {
  if (![self instanceActive])
    return nil;

  std::u16string textContent = _owner->GetTextContentUTF16();
  if (NSMaxRange(range) > textContent.length())
    return nil;

  // We potentially need to add text attributes to the whole text content
  // because a spelling mistake might start or end outside the given range.
  NSMutableAttributedString* attributedTextContent =
      [[[NSMutableAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(textContent)] autorelease];
  if (!_owner->IsText()) {
    BrowserAccessibility::AXRange ax_range(
        _owner->CreateTextPositionAt(0),
        _owner->CreateTextPositionAt(static_cast<int>(textContent.length())));
    AddMisspelledTextAttributes(ax_range, attributedTextContent);
  }

  return [attributedTextContent attributedSubstringFromRange:range];
}

- (NSRect)frameForRange:(NSRange)range {
  if (!_owner->IsText() && !_owner->IsAtomicTextField())
    return CGRectNull;
  gfx::Rect rect = _owner->GetUnclippedRootFrameInnerTextRangeBoundsRect(
      range.location, NSMaxRange(range));
  return [self rectInScreen:rect];
}

// Returns the accessibility value for the given attribute.  If the value isn't
// supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityAttributeValue",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return nil;

  SEL selector = NSSelectorFromString([self methodNameForAttribute:attribute]);
  if (selector)
    return [self performSelector:selector];

  return [super accessibilityAttributeValue:attribute];
}

- (id)AXStringForRange:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  return [self valueForRange:[(NSValue*)parameter rangeValue]];
}

- (id)AXAttributedStringForRange:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  return [self attributedValueForRange:[(NSValue*)parameter rangeValue]];
}

- (id)AXLineForIndex:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  int lineIndex = [(NSNumber*)parameter intValue];
  const std::vector<int> lineStarts =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
  auto iterator =
      std::lower_bound(lineStarts.begin(), lineStarts.end(), lineIndex);
  return @(std::distance(lineStarts.begin(), iterator));
}

- (id)AXRangeForLine:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  if (!_owner->IsTextField())
    return nil;

  int lineIndex = [(NSNumber*)parameter intValue];
  const std::vector<int> lineStarts =
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
  std::u16string value = _owner->GetValueForControl();
  int valueLength = static_cast<int>(value.size());

  int lineCount = static_cast<int>(lineStarts.size());
  if (lineIndex < 0 || lineIndex >= lineCount)
    return nil;
  int start = lineStarts[lineIndex];
  int end =
      (lineIndex < (lineCount - 1)) ? lineStarts[lineIndex + 1] : valueLength;
  return [NSValue valueWithRange:NSMakeRange(start, end - start)];
}

// Returns the accessibility value for the given attribute and parameter. If the
// value isn't supported this will return nil.
- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (parameter && [parameter isKindOfClass:[NSNumber self]]) {
    TRACE_EVENT2(
        "accessibility",
        "BrowserAccessibilityCocoa::accessibilityAttributeValue:forParameter",
        "role=", ui::ToString([self internalRole]), "attribute=",
        base::SysNSStringToUTF8(attribute) +
            " parameter=" + base::SysNSStringToUTF8([parameter stringValue]));
  } else {
    TRACE_EVENT2(
        "accessibility",
        "BrowserAccessibilityCocoa::accessibilityAttributeValue:forParameter",
        "role=", ui::ToString([self internalRole]),
        "attribute=", base::SysNSStringToUTF8(attribute));
  }

  if (![self instanceActive])
    return nil;

  if ([attribute isEqualToString:
                     NSAccessibilityStringForRangeParameterizedAttribute]) {
    return [self AXStringForRange:parameter];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityAttributedStringForRangeParameterizedAttribute]) {
    return [self AXAttributedStringForRange:parameter];
  }

  if ([attribute
          isEqualToString:NSAccessibilityLineForIndexParameterizedAttribute]) {
    return [self AXLineForIndex:parameter];
  }

  if ([attribute
          isEqualToString:NSAccessibilityRangeForLineParameterizedAttribute]) {
    return [self AXRangeForLine:parameter];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityCellForColumnAndRowParameterizedAttribute]) {
    if (!ui::IsTableLike([self internalRole]))
      return nil;
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;
    if (2 != [parameter count])
      return nil;
    NSArray* array = parameter;
    int column = [[array objectAtIndex:0] intValue];
    int row = [[array objectAtIndex:1] intValue];

    ui::AXNode* cellNode = _owner->node()->GetTableCellFromCoords(row, column);
    if (!cellNode)
      return nil;

    BrowserAccessibility* cell = _owner->manager()->GetFromID(cellNode->id());
    if (cell)
      return ToBrowserAccessibilityCocoa(cell);
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityUIElementForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (!position->IsNullPosition()) {
      BrowserAccessibility* ui_element =
          _owner->manager()->GetFromAXNode(position->GetAnchor());
      if (ui_element)
        return ToBrowserAccessibilityCocoa(ui_element);
    }

    return nil;
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute]) {
    BrowserAccessibility::AXPosition startPosition =
        _owner->CreateTextPositionAt(0);
    BrowserAccessibility::AXPosition endPosition =
        startPosition->CreatePositionAtEndOfAnchor();
    BrowserAccessibility::AXRange range = BrowserAccessibility::AXRange(
        std::move(startPosition), std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityStringForTextMarkerRangeParameterizedAttribute])
    return GetTextForTextMarkerRange(parameter);

  if ([attribute
          isEqualToString:
              NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute])
    return GetAttributedTextForTextMarkerRange(parameter);

  if ([attribute
          isEqualToString:
              NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextCharacterPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousCharacterPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition endPosition =
        CreatePositionFromTextMarker(parameter);
    if (endPosition->IsNullPosition())
      return nil;

    BrowserAccessibility::AXPosition startWordPosition =
        endPosition->CreatePreviousWordStartPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibility::AXPosition endWordPosition =
        endPosition->CreatePreviousWordEndPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibility::AXPosition startPosition =
        *startWordPosition <= *endWordPosition ? std::move(endWordPosition)
                                               : std::move(startWordPosition);
    BrowserAccessibility::AXRange range(std::move(startPosition),
                                        std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition startPosition =
        CreatePositionFromTextMarker(parameter);
    if (startPosition->IsNullPosition())
      return nil;

    BrowserAccessibility::AXPosition endWordPosition =
        startPosition->CreateNextWordEndPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibility::AXPosition startWordPosition =
        startPosition->CreateNextWordStartPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibility::AXPosition endPosition =
        *startWordPosition <= *endWordPosition ? std::move(startWordPosition)
                                               : std::move(endWordPosition);
    BrowserAccessibility::AXRange range(std::move(startPosition),
                                        std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextWordEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousWordStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute isEqualToString:
                     NSAccessibilityLineForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    int textOffset = position->AsTextPosition()->text_offset();
    const std::vector<int> lineStarts =
        _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
    const auto iterator =
        std::lower_bound(lineStarts.begin(), lineStarts.end(), textOffset);
    return @(std::distance(lineStarts.begin(), iterator));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForLineParameterizedAttribute]) {
    int lineIndex = [(NSNumber*)parameter intValue];
    const std::vector<int> lineStarts =
        _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLineStarts);
    int lineCount = static_cast<int>(lineStarts.size());
    if (lineIndex < 0 || lineIndex >= lineCount)
      return nil;

    int lineStartOffset = lineStarts[lineIndex];
    BrowserAccessibility::AXPosition lineStartPosition = CreateTextPosition(
        *_owner, lineStartOffset, ax::mojom::TextAffinity::kDownstream);
    if (lineStartPosition->IsNullPosition())
      return nil;

    // Make sure that the line start position is really at the start of the
    // current line.
    lineStartPosition = lineStartPosition->CreatePreviousLineStartPosition(
        ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibility::AXPosition lineEndPosition =
        lineStartPosition->CreateNextLineEndPosition(
            ui::AXBoundaryBehavior::StopAtAnchorBoundary);
    BrowserAccessibility::AXRange range(std::move(lineStartPosition),
                                        std::move(lineEndPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition endPosition =
        CreatePositionFromTextMarker(parameter);
    if (endPosition->IsNullPosition())
      return nil;

    BrowserAccessibility::AXPosition startLinePosition =
        endPosition->CreatePreviousLineStartPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibility::AXPosition endLinePosition =
        endPosition->CreatePreviousLineEndPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibility::AXPosition startPosition =
        *startLinePosition <= *endLinePosition ? std::move(endLinePosition)
                                               : std::move(startLinePosition);
    BrowserAccessibility::AXRange range(std::move(startPosition),
                                        std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition startPosition =
        CreatePositionFromTextMarker(parameter);
    if (startPosition->IsNullPosition())
      return nil;

    BrowserAccessibility::AXPosition startLinePosition =
        startPosition->CreateNextLineStartPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibility::AXPosition endLinePosition =
        startPosition->CreateNextLineEndPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibility::AXPosition endPosition =
        *startLinePosition <= *endLinePosition ? std::move(startLinePosition)
                                               : std::move(endLinePosition);
    BrowserAccessibility::AXRange range(std::move(startPosition),
                                        std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextLineEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousLineStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    BrowserAccessibility::AXPosition startPosition =
        position->CreatePreviousParagraphStartPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibility::AXPosition endPosition =
        position->CreateNextParagraphEndPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibility::AXRange range(std::move(startPosition),
                                        std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreateNextParagraphEndPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return CreateTextMarker(position->CreatePreviousParagraphStartPosition(
        ui::AXBoundaryBehavior::CrossBoundary));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    BrowserAccessibility::AXPosition startPosition =
        position->CreatePreviousFormatStartPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibility::AXPosition endPosition =
        position->CreateNextFormatEndPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibility::AXRange range(std::move(startPosition),
                                        std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute]) {
    NSString* text = GetTextForTextMarkerRange(parameter);
    return @([text length]);
  }

  if ([attribute isEqualToString:
                     NSAccessibilityTextMarkerIsValidParameterizedAttribute]) {
    return @(CreatePositionFromTextMarker(parameter)->IsNullPosition());
  }

  if ([attribute isEqualToString:
                     NSAccessibilityIndexForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;
    return @(position->AsTextPosition()->text_offset());
  }

  if ([attribute isEqualToString:
                     NSAccessibilityTextMarkerForIndexParameterizedAttribute]) {
    int index = [static_cast<NSNumber*>(parameter) intValue];
    if (index < 0)
      return nil;

    const BrowserAccessibility* root = _owner->manager()->GetRoot();
    if (!root)
      return nil;

    return CreateTextMarker(root->CreateTextPositionAt(index));
  }

  if ([attribute isEqualToString:
                     NSAccessibilityBoundsForRangeParameterizedAttribute]) {
    NSRect rect = [self frameForRange:[(NSValue*)parameter rangeValue]];
    return CGRectIsNull(rect) ? nil : [NSValue valueWithRect:rect];
  }

  if ([attribute isEqualToString:
                   NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute]) {
    OneShotAccessibilityTreeSearch search(_owner);
    if (InitializeAccessibilityTreeSearch(&search, parameter))
      return @(search.CountMatches());
    return nil;
  }

  if ([attribute isEqualToString:
                     NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute]) {
    OneShotAccessibilityTreeSearch search(_owner);
    if (InitializeAccessibilityTreeSearch(&search, parameter)) {
      size_t count = search.CountMatches();
      NSMutableArray* result = [NSMutableArray arrayWithCapacity:count];
      for (size_t i = 0; i < count; ++i) {
        BrowserAccessibility* match = search.GetMatchAtIndex(i);
        [result addObject:ToBrowserAccessibilityCocoa(match)];
      }
      return result;
    }
    return nil;
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return nil;

    // If the initial position is between lines, e.g. if it is on a soft line
    // break or on an ignored position that separates lines, we have to return
    // the previous line. This is what Safari does.
    //
    // Note that hard line breaks are on a line of their own.
    BrowserAccessibility::AXPosition startPosition =
        position->CreatePreviousLineStartPosition(
            ui::AXBoundaryBehavior::StopIfAlreadyAtBoundary);
    BrowserAccessibility::AXPosition endPosition =
        startPosition->CreateNextLineStartPosition(
            ui::AXBoundaryBehavior::StopAtLastAnchorBoundary);
    BrowserAccessibility::AXRange range(std::move(startPosition),
                                        std::move(endPosition));
    return CreateTextMarkerRange(std::move(range));
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute]) {
    BrowserAccessibility* startObject;
    BrowserAccessibility* endObject;
    int startOffset, endOffset;
    BrowserAccessibility::AXRange range =
        CreateRangeFromTextMarkerRange(parameter);
    if (range.IsNull())
      return nil;

    startObject = _owner->manager()->GetFromAXNode(range.anchor()->GetAnchor());
    endObject = _owner->manager()->GetFromAXNode(range.focus()->GetAnchor());
    startOffset = range.anchor()->text_offset();
    endOffset = range.focus()->text_offset();
    DCHECK(startObject && endObject);
    DCHECK_GE(startOffset, 0);
    DCHECK_GE(endOffset, 0);
    if (!startObject || !endObject || startOffset < 0 || endOffset < 0)
      return nil;

    gfx::Rect rect =
        BrowserAccessibilityManager::GetRootFrameInnerTextRangeBoundsRect(
            *startObject, startOffset, *endObject, endOffset);
    NSRect nsrect = [self rectInScreen:rect];
    return [NSValue valueWithRect:nsrect];
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute]) {
    if (![parameter isKindOfClass:[NSArray class]])
      return nil;

    NSArray* textMarkerArray = parameter;
    if ([textMarkerArray count] != 2)
      return nil;

    BrowserAccessibility::AXPosition startPosition =
        CreatePositionFromTextMarker([textMarkerArray objectAtIndex:0]);
    BrowserAccessibility::AXPosition endPosition =
        CreatePositionFromTextMarker([textMarkerArray objectAtIndex:1]);
    if (*startPosition <= *endPosition) {
      return CreateTextMarkerRange(BrowserAccessibility::AXRange(
          std::move(startPosition), std::move(endPosition)));
    } else {
      return CreateTextMarkerRange(BrowserAccessibility::AXRange(
          std::move(endPosition), std::move(startPosition)));
    }
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerDebugDescriptionParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    return base::SysUTF8ToNSString(position->ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerRangeDebugDescriptionParameterizedAttribute]) {
    BrowserAccessibility::AXRange range =
        CreateRangeFromTextMarkerRange(parameter);
    return base::SysUTF8ToNSString(range.ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityTextMarkerNodeDebugDescriptionParameterizedAttribute]) {
    BrowserAccessibility::AXPosition position =
        CreatePositionFromTextMarker(parameter);
    if (position->IsNullPosition())
      return @"nil";
    DCHECK(position->GetAnchor());
    return base::SysUTF8ToNSString(position->GetAnchor()->data().ToString());
  }

  if ([attribute
          isEqualToString:
              NSAccessibilityIndexForChildUIElementParameterizedAttribute]) {
    if (![parameter isKindOfClass:[BrowserAccessibilityCocoa class]])
      return nil;

    BrowserAccessibilityCocoa* childCocoaObj =
        (BrowserAccessibilityCocoa*)parameter;
    BrowserAccessibility* child = [childCocoaObj owner];
    if (!child)
      return nil;

    if (child->PlatformGetParent() != _owner)
      return nil;

    return @(child->GetIndexInParent());
  }

  return nil;
}

// Returns an array of parameterized attributes names that this object will
// respond to.
- (NSArray*)accessibilityParameterizedAttributeNames {
  TRACE_EVENT1(
      "accessibility",
      "BrowserAccessibilityCocoa::accessibilityParameterizedAttributeNames",
      "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray
      arrayWithObjects:
          NSAccessibilityUIElementForTextMarkerParameterizedAttribute,
          NSAccessibilityTextMarkerRangeForUIElementParameterizedAttribute,
          NSAccessibilityLineForTextMarkerParameterizedAttribute,
          NSAccessibilityTextMarkerRangeForLineParameterizedAttribute,
          NSAccessibilityStringForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityTextMarkerForPositionParameterizedAttribute,
          NSAccessibilityBoundsForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityAttributedStringForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityAttributedStringForTextMarkerRangeWithOptionsParameterizedAttribute,
          NSAccessibilityTextMarkerRangeForUnorderedTextMarkersParameterizedAttribute,
          NSAccessibilityNextTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityLeftWordTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityRightWordTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityLeftLineTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityRightLineTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilitySentenceTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityParagraphTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityNextWordEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousWordStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityNextLineEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousLineStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityNextSentenceEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousSentenceStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityNextParagraphEndTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityPreviousParagraphStartTextMarkerForTextMarkerParameterizedAttribute,
          NSAccessibilityStyleTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityLengthForTextMarkerRangeParameterizedAttribute,
          NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute,
          NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute,
          NSAccessibilityLineTextMarkerRangeForTextMarkerParameterizedAttribute,
          NSAccessibilityIndexForChildUIElementParameterizedAttribute,
          NSAccessibilityBoundsForRangeParameterizedAttribute,
          NSAccessibilityStringForRangeParameterizedAttribute,
          NSAccessibilityUIElementCountForSearchPredicateParameterizedAttribute,
          NSAccessibilityUIElementsForSearchPredicateParameterizedAttribute,
          NSAccessibilitySelectTextWithCriteriaParameterizedAttribute, nil];

  if ([[self role] isEqualToString:NSAccessibilityTableRole] ||
      [[self role] isEqualToString:NSAccessibilityGridRole]) {
    [ret addObject:NSAccessibilityCellForColumnAndRowParameterizedAttribute];
  }

  if (_owner->HasState(ax::mojom::State::kEditable)) {
    [ret addObjectsFromArray:@[
      NSAccessibilityLineForIndexParameterizedAttribute,
      NSAccessibilityRangeForLineParameterizedAttribute,
      NSAccessibilityStringForRangeParameterizedAttribute,
      NSAccessibilityRangeForPositionParameterizedAttribute,
      NSAccessibilityRangeForIndexParameterizedAttribute,
      NSAccessibilityBoundsForRangeParameterizedAttribute,
      NSAccessibilityRTFForRangeParameterizedAttribute,
      NSAccessibilityAttributedStringForRangeParameterizedAttribute,
      NSAccessibilityStyleRangeForIndexParameterizedAttribute
    ]];
  }

  if ([self internalRole] == ax::mojom::Role::kStaticText)
    [ret addObject:NSAccessibilityBoundsForRangeParameterizedAttribute];

  if (ui::IsPlatformDocument(_owner->GetRole())) {
    [ret addObjectsFromArray:@[
      NSAccessibilityTextMarkerIsValidParameterizedAttribute,
      NSAccessibilityIndexForTextMarkerParameterizedAttribute,
      NSAccessibilityTextMarkerForIndexParameterizedAttribute
    ]];
  }

  return ret;
}

// Returns an array of action names that this object will respond to.
- (NSArray*)accessibilityActionNames {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityActionNames",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  NSMutableArray* actions = [NSMutableArray
      arrayWithObjects:NSAccessibilityShowMenuAction,
                       NSAccessibilityScrollToVisibleAction, nil];

  // VoiceOver expects the "press" action to be first.
  if (_owner->IsClickable())
    [actions insertObject:NSAccessibilityPressAction atIndex:0];

  if (ui::IsMenuRelated(_owner->GetRole()))
    [actions addObject:NSAccessibilityCancelAction];

  if ([self internalRole] == ax::mojom::Role::kSlider ||
      [self internalRole] == ax::mojom::Role::kSpinButton) {
    [actions addObjectsFromArray:@[
      NSAccessibilityIncrementAction, NSAccessibilityDecrementAction
    ]];
  }

  return actions;
}

// Returns the list of accessibility attributes that this object supports.
- (NSArray*)accessibilityAttributeNames {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityAttributeNames",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  // General attributes.
  NSMutableArray* ret = [NSMutableArray
      arrayWithObjects:NSAccessibilityBlockQuoteLevelAttribute,
                       NSAccessibilityChildrenAttribute,
                       NSAccessibilityIdentifierChromeAttribute,
                       NSAccessibilityDescriptionAttribute,
                       NSAccessibilityDOMClassList,
                       NSAccessibilityElementBusyAttribute,
                       NSAccessibilityEnabledAttribute,
                       NSAccessibilityEndTextMarkerAttribute,
                       NSAccessibilityFocusedAttribute,
                       NSAccessibilityHelpAttribute,
                       NSAccessibilityLinkedUIElementsAttribute,
                       NSAccessibilityParentAttribute,
                       NSAccessibilityPositionAttribute,
                       NSAccessibilityRoleAttribute,
                       NSAccessibilityRoleDescriptionAttribute,
                       NSAccessibilitySelectedAttribute,
                       NSAccessibilitySelectedTextMarkerRangeAttribute,
                       NSAccessibilitySizeAttribute,
                       NSAccessibilityStartTextMarkerAttribute,
                       NSAccessibilitySubroleAttribute,
                       NSAccessibilityTitleAttribute,
                       NSAccessibilityTitleUIElementAttribute,
                       NSAccessibilityTopLevelUIElementAttribute,
                       NSAccessibilityValueAttribute,
                       NSAccessibilityVisitedAttribute,
                       NSAccessibilityWindowAttribute, nil];

  // Specific role attributes.
  NSString* role = [self role];
  NSString* subrole = [self subrole];
  if ([role isEqualToString:NSAccessibilityTableRole] ||
      [role isEqualToString:NSAccessibilityGridRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityColumnsAttribute,
      NSAccessibilityVisibleColumnsAttribute,
      NSAccessibilityRowsAttribute,
      NSAccessibilityVisibleRowsAttribute,
      NSAccessibilityVisibleCellsAttribute,
      NSAccessibilityHeaderAttribute,
      NSAccessibilityRowHeaderUIElementsAttribute,
    ]];
  } else if ([role isEqualToString:NSAccessibilityColumnRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityIndexAttribute, NSAccessibilityHeaderAttribute,
      NSAccessibilityRowsAttribute, NSAccessibilityVisibleRowsAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityCellRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilityColumnIndexRangeAttribute,
      NSAccessibilityRowIndexRangeAttribute,
      @"AXSortDirection",
    ]];
    if ([self internalRole] != ax::mojom::Role::kRowHeader)
      [ret addObject:NSAccessibilityRowHeaderUIElementsAttribute];
  } else if ([role isEqualToString:@"AXWebArea"]) {
    [ret addObjectsFromArray:@[
      @"AXLoaded", NSAccessibilityLoadingProgressAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityTabGroupRole]) {
    [ret addObject:NSAccessibilityTabsAttribute];
  } else if (_owner->GetData().IsRangeValueSupported()) {
    [ret addObjectsFromArray:@[
      NSAccessibilityMaxValueAttribute, NSAccessibilityMinValueAttribute,
      NSAccessibilityValueDescriptionAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityRowRole]) {
    BrowserAccessibility* container = _owner->PlatformGetParent();
    if (container && container->GetRole() == ax::mojom::Role::kRowGroup)
      container = container->PlatformGetParent();
    if ([subrole isEqualToString:NSAccessibilityOutlineRowSubrole] ||
        (container && container->GetRole() == ax::mojom::Role::kTreeGrid)) {
      // clang-format off
      [ret addObjectsFromArray:@[
        NSAccessibilityIndexAttribute,
        NSAccessibilityDisclosedByRowAttribute,
        NSAccessibilityDisclosedRowsAttribute,
        NSAccessibilityDisclosingAttribute,
        NSAccessibilityDisclosureLevelAttribute
      ]];
      // clang-format on
    } else {
      [ret addObject:NSAccessibilityIndexAttribute];
    }
  } else if ([self internalRole] == ax::mojom::Role::kHeading) {
    // Heading level is exposed in both AXDisclosureLevel and AXValue.
    // Safari also exposes the level in both.
    [ret addObject:NSAccessibilityDisclosureLevelAttribute];
  } else if ([role isEqualToString:NSAccessibilityListRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilitySelectedChildrenAttribute,
      NSAccessibilityVisibleChildrenAttribute
    ]];
  } else if ([role isEqualToString:NSAccessibilityOutlineRole]) {
    [ret addObjectsFromArray:@[
      NSAccessibilitySelectedRowsAttribute, NSAccessibilityRowsAttribute,
      NSAccessibilityColumnsAttribute, NSAccessibilityOrientationAttribute
    ]];
  }

  // Caret navigation and text selection attributes.
  if (_owner->HasState(ax::mojom::State::kEditable)) {
    // Add ancestor attributes if not a web area.
    if (!ui::IsPlatformDocument(_owner->GetRole())) {
      [ret addObjectsFromArray:@[
        NSAccessibilityEditableAncestorAttribute,
        NSAccessibilityFocusableAncestorAttribute,
        NSAccessibilityHighestEditableAncestorAttribute
      ]];
    }
  }

  if (_owner->IsTextField()) {
    [ret addObjectsFromArray:@[
      NSAccessibilityInsertionPointLineNumberAttribute,
      NSAccessibilityNumberOfCharactersAttribute,
      NSAccessibilityPlaceholderValueAttribute,
      NSAccessibilitySelectedTextAttribute,
      NSAccessibilitySelectedTextRangeAttribute,
      NSAccessibilityVisibleCharacterRangeAttribute,
      NSAccessibilityValueAutofillAvailableAttribute,
      // Not currently supported by Chrome:
      // NSAccessibilityValueAutofilledAttribute,
      // Not currently supported by Chrome:
      // NSAccessibilityValueAutofillTypeAttribute
    ]];
  }

  std::string dropEffect;
  if (_owner->GetHtmlAttribute("aria-dropeffect", &dropEffect))
    [ret addObject:NSAccessibilityDropEffectsAttribute];

  std::string grabbed;
  if (_owner->GetHtmlAttribute("aria-grabbed", &grabbed))
    [ret addObject:NSAccessibilityGrabbedAttribute];

  if (_owner->HasBoolAttribute(ax::mojom::BoolAttribute::kSelected))
    [ret addObject:NSAccessibilitySelectedAttribute];

  if (GetState(_owner, ax::mojom::State::kExpanded) ||
      GetState(_owner, ax::mojom::State::kCollapsed)) {
    [ret addObject:NSAccessibilityExpandedAttribute];
  }

  if (GetState(_owner, ax::mojom::State::kVertical) ||
      GetState(_owner, ax::mojom::State::kHorizontal)) {
    [ret addObject:NSAccessibilityOrientationAttribute];
  }

  // TODO(accessibility) What nodes should language be exposed on given new
  // auto detection features?
  //
  // Once lang attribute inheritance becomes stable most nodes will have a
  // language, so it may make more sense to always expose this attribute.
  //
  // For now we expose the language attribute if we have any language set.
  if (_owner->node() && !_owner->node()->GetLanguage().empty())
    [ret addObject:NSAccessibilityLanguageAttribute];

  if ([self internalRole] == ax::mojom::Role::kTextFieldWithComboBox)
    [ret addObject:NSAccessibilityOwnsAttribute];

  // Title UI Element.
  if (_owner->HasIntListAttribute(
          ax::mojom::IntListAttribute::kLabelledbyIds) &&
      _owner->GetIntListAttribute(ax::mojom::IntListAttribute::kLabelledbyIds)
              .size() > 0) {
    [ret addObject:NSAccessibilityTitleUIElementAttribute];
  }

  if ([self shouldExposeTitleUIElement])
    [ret addObject:NSAccessibilityTitleUIElementAttribute];

  if (_owner->HasStringAttribute(ax::mojom::StringAttribute::kKeyShortcuts))
    [ret addObject:NSAccessibilityKeyShortcutsValueAttribute];

  // TODO(aboxhall): expose NSAccessibilityServesAsTitleForUIElementsAttribute
  // for elements which are referred to by labelledby or are labels

  NSArray* super_ret = [super accessibilityAttributeNames];
  [ret addObjectsFromArray:super_ret];
  return ret;
}

// Returns the index of the child in this objects array of children.
- (NSUInteger)accessibilityGetIndexOf:(id)child {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityGetIndexOf",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return 0;

  NSUInteger index = 0;
  for (BrowserAccessibilityCocoa* childToCheck in [self children]) {
    if ([child isEqual:childToCheck])
      return index;
    ++index;
  }
  return NSNotFound;
}

// Returns whether or not the specified attribute can be set by the
// accessibility API via |accessibilitySetValue:forAttribute:|.
- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityIsAttributeSettable",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return NO;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    if ([self internalRole] == ax::mojom::Role::kDateTime)
      return NO;

    return GetState(_owner, ax::mojom::State::kFocusable);
  }

  if ([attribute isEqualToString:NSAccessibilityValueAttribute])
    return _owner->HasAction(ax::mojom::Action::kSetValue);

  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute] &&
      _owner->HasState(ax::mojom::State::kEditable)) {
    return YES;
  }

  if ([attribute
          isEqualToString:NSAccessibilitySelectedTextMarkerRangeAttribute])
    return YES;

  return NO;
}

// Returns whether or not this object should be ignored in the accessibility
// tree.
- (BOOL)accessibilityIsIgnored {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityIsIgnored",
               "role=", ui::ToString([self internalRole]));
  return [self isIgnored];
}

- (BOOL)isAccessibilityElement {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::isAccessibilityElement",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return NO;

  // Unlike accessibilityIsIgnored do not return false for invisible elements,
  // otherwise it fails to fire events for menus.
  return ![[self role] isEqualToString:NSAccessibilityUnknownRole] &&
         !_owner->IsIgnored();
}

- (BOOL)isAccessibilityEnabled {
  if (![self instanceActive])
    return NO;

  return _owner->GetData().GetRestriction() !=
         ax::mojom::Restriction::kDisabled;
}

- (NSRect)accessibilityFrame {
  if (![self instanceActive])
    return NSZeroRect;

  BrowserAccessibilityManager* manager = _owner->manager();
  auto rect = _owner->GetBoundsRect(ui::AXCoordinateSystem::kScreenDIPs,
                                    ui::AXClippingBehavior::kClipped);

  // Convert to DIPs if UseZoomForDSF is enabled.
  // TODO(vmpstr): GetBoundsRect() call above should account for this instead.
  auto result_rect =
      IsUseZoomForDSFEnabled()
          ? ScaleToRoundedRect(rect, 1.f / manager->device_scale_factor())
          : rect;

  return gfx::ScreenRectToNSRect(result_rect);
}

- (BOOL)isCheckable {
  if (![self instanceActive])
    return NO;

  return _owner->GetData().HasCheckedState() ||
         _owner->GetRole() == ax::mojom::Role::kTab;
}

// Performs the given accessibility action on the webkit accessibility object
// that backs this object.
- (void)accessibilityPerformAction:(NSString*)action {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityPerformAction",
               "role=", ui::ToString([self internalRole]),
               "action=", base::SysNSStringToUTF8(action));
  if (![self instanceActive])
    return;

  // TODO(dmazzoni): Support more actions.
  BrowserAccessibility* actionTarget = [self actionTarget];
  BrowserAccessibilityManager* manager = actionTarget->manager();
  if ([action isEqualToString:NSAccessibilityPressAction]) {
    manager->DoDefaultAction(*actionTarget);
    if (actionTarget->GetData().GetRestriction() !=
            ax::mojom::Restriction::kNone ||
        ![self isCheckable])
      return;
    // Hack: preemptively set the checked state to what it should become,
    // otherwise VoiceOver will very likely report the old, incorrect state to
    // the user as it requests the value too quickly.
    ui::AXNode* node = actionTarget->node();
    if (!node)
      return;
    AXNodeData data(node->TakeData());  // Temporarily take data.
    if (data.role == ax::mojom::Role::kRadioButton) {
      data.SetCheckedState(ax::mojom::CheckedState::kTrue);
    } else if (data.role == ax::mojom::Role::kCheckBox ||
               data.role == ax::mojom::Role::kSwitch ||
               data.role == ax::mojom::Role::kToggleButton) {
      ax::mojom::CheckedState checkedState = data.GetCheckedState();
      ax::mojom::CheckedState newCheckedState =
          checkedState == ax::mojom::CheckedState::kFalse
              ? ax::mojom::CheckedState::kTrue
              : ax::mojom::CheckedState::kFalse;
      data.SetCheckedState(newCheckedState);
    }
    node->SetData(data);  // Set the data back in the node.
  } else if ([action isEqualToString:NSAccessibilityShowMenuAction]) {
    manager->ShowContextMenu(*actionTarget);
  } else if ([action isEqualToString:NSAccessibilityScrollToVisibleAction]) {
    manager->ScrollToMakeVisible(*actionTarget, gfx::Rect());
  } else if ([action isEqualToString:NSAccessibilityIncrementAction]) {
    manager->Increment(*actionTarget);
  } else if ([action isEqualToString:NSAccessibilityDecrementAction]) {
    manager->Decrement(*actionTarget);
  }
}

// Returns the description of the given action.
- (NSString*)accessibilityActionDescription:(NSString*)action {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityActionDescription",
               "role=", ui::ToString([self internalRole]),
               "action=", base::SysNSStringToUTF8(action));
  if (![self instanceActive])
    return nil;

  return NSAccessibilityActionDescription(action);
}

// Sets an override value for a specific accessibility attribute.
// This class does not support this.
- (BOOL)accessibilitySetOverrideValue:(id)value
                         forAttribute:(NSString*)attribute {
  TRACE_EVENT2(
      "accessibility",
      "BrowserAccessibilityCocoa::accessibilitySetOverrideValue:forAttribute",
      "role=", ui::ToString([self internalRole]),
      "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return NO;
  return NO;
}

// Sets the value for an accessibility attribute via the accessibility API.
- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilitySetValue:forAttribute",
               "role=", ui::ToString([self internalRole]),
               "attribute=", base::SysNSStringToUTF8(attribute));
  if (![self instanceActive])
    return;

  if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    NSNumber* focusedNumber = value;
    BrowserAccessibilityManager* manager = _owner->manager();
    BOOL focused = [focusedNumber intValue];
    if (focused)
      manager->SetFocus(*_owner);
  }
  if ([attribute isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    if (content::IsNSRange(value)) {
      NSRange range = [(NSValue*)value rangeValue];
      BrowserAccessibilityManager* manager = _owner->manager();
      manager->SetSelection(BrowserAccessibility::AXRange(
          _owner->CreateTextPositionAt(range.location)
              ->AsTextSelectionPosition(),
          _owner->CreateTextPositionAt(NSMaxRange(range))
              ->AsTextSelectionPosition()));
    }
  }
  if ([attribute
          isEqualToString:NSAccessibilitySelectedTextMarkerRangeAttribute]) {
    BrowserAccessibility::AXRange range = CreateRangeFromTextMarkerRange(value);
    if (range.IsNull())
      return;
    BrowserAccessibilityManager* manager = _owner->manager();
    manager->SetSelection(BrowserAccessibility::AXRange(
        range.anchor()->AsTextSelectionPosition(),
        range.focus()->AsTextSelectionPosition()));
  }
}

- (id)accessibilityFocusedUIElement {
  TRACE_EVENT1("accessibility",
               "BrowserAccessibilityCocoa::accessibilityFocusedUIElement",
               "role=", ui::ToString([self internalRole]));
  if (![self instanceActive])
    return nil;

  return ToBrowserAccessibilityCocoa(_owner->manager()->GetFocus());
}

// Returns the deepest accessibility child that should not be ignored.
// It is assumed that the hit test has been narrowed down to this object
// or one of its children, so this will never return nil unless this
// object is invalid.
- (id)accessibilityHitTest:(NSPoint)point {
  TRACE_EVENT2("accessibility",
               "BrowserAccessibilityCocoa::accessibilityHitTest",
               "role=", ui::ToString([self internalRole]),
               "point=", base::SysNSStringToUTF8(NSStringFromPoint(point)));
  if (![self instanceActive])
    return nil;

  // The point we receive is in frame coordinates.
  // Convert to screen coordinates and then to physical pixel coordinates.
  BrowserAccessibilityManager* manager = _owner->manager();
  gfx::Point screen_point_in_dips(point.x, point.y);

  auto offset_in_blink_space =
      manager->GetViewBoundsInScreenCoordinates().OffsetFromOrigin();

  // If UseZoomForDSF is enabled, blink space is physical, so we scale the
  // point first then add the offset. Otherwise, it's in DIPs so we add the
  // offset first and then scale.
  // TODO(vmpstr): GetViewBoundsInScreenCoordinates should return consistent
  // space.
  gfx::Point screen_point_in_physical_space;
  if (IsUseZoomForDSFEnabled()) {
    screen_point_in_physical_space = ScaleToRoundedPoint(
        screen_point_in_dips, manager->device_scale_factor());
    screen_point_in_physical_space += offset_in_blink_space;
  } else {
    screen_point_in_dips += offset_in_blink_space;
    screen_point_in_physical_space = ScaleToRoundedPoint(
        screen_point_in_dips, manager->device_scale_factor());
  }

  BrowserAccessibility* hit =
      manager->CachingAsyncHitTest(screen_point_in_physical_space);
  if (!hit)
    return nil;

  return NSAccessibilityUnignoredAncestor(ToBrowserAccessibilityCocoa(hit));
}

- (BOOL)isEqual:(id)object {
  if (![object isKindOfClass:[BrowserAccessibilityCocoa class]])
    return NO;
  return ([self hash] == [object hash]);
}

- (NSUInteger)hash {
  // Potentially called during dealloc.
  if (![self instanceActive])
    return [super hash];
  return _owner->GetId();
}

- (NSString*)internalId {
  return [@(_owner->GetId()) stringValue];
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  TRACE_EVENT0("accessibility",
               "BrowserAccessibilityCocoa::accessibilityNotifiesWhenDestroyed");
  // Indicate that BrowserAccessibilityCocoa will post a notification when it's
  // destroyed (see -detach). This allows VoiceOver to do some internal things
  // more efficiently.
  return YES;
}

// Choose the appropriate accessibility object to receive an action depending
// on the characteristics of this accessibility node.
- (BrowserAccessibility*)actionTarget {
  // When an action is triggered on a container with selectable children and
  // one of those children is an active descendant or focused, retarget the
  // action to that child. See https://crbug.com/1114892.
  if (!ui::IsContainerWithSelectableChildren(_owner->GetRole()))
    return _owner;

  // Active descendant takes priority over focus, because the webpage author has
  // explicitly designated a different behavior for users of assistive software.
  BrowserAccessibility* activeDescendant =
      _owner->manager()->GetActiveDescendant(_owner);
  if (activeDescendant != _owner)
    return activeDescendant;

  BrowserAccessibility* focus = _owner->manager()->GetFocus();
  if (focus && focus->IsDescendantOf(_owner))
    return focus;

  return _owner;
}

@end
