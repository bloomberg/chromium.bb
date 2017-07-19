// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/ax_platform_node_mac.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/strings/grit/ui_strings.h"

namespace {

using RoleMap = std::map<ui::AXRole, NSString*>;
using EventMap = std::map<ui::AXEvent, NSString*>;
using ActionList = std::vector<std::pair<ui::AXAction, NSString*>>;

RoleMap BuildRoleMap() {
  const RoleMap::value_type roles[] = {
      {ui::AX_ROLE_ABBR, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ALERT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ALERT_DIALOG, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ANCHOR, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ANNOTATION, NSAccessibilityUnknownRole},
      {ui::AX_ROLE_APPLICATION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ARTICLE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_AUDIO, NSAccessibilityGroupRole},
      {ui::AX_ROLE_BANNER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_BLOCKQUOTE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_BUSY_INDICATOR, NSAccessibilityBusyIndicatorRole},
      {ui::AX_ROLE_BUTTON, NSAccessibilityButtonRole},
      {ui::AX_ROLE_CANVAS, NSAccessibilityImageRole},
      {ui::AX_ROLE_CAPTION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_CELL, @"AXCell"},
      {ui::AX_ROLE_CHECK_BOX, NSAccessibilityCheckBoxRole},
      {ui::AX_ROLE_COLOR_WELL, NSAccessibilityColorWellRole},
      {ui::AX_ROLE_COLUMN, NSAccessibilityColumnRole},
      {ui::AX_ROLE_COLUMN_HEADER, @"AXCell"},
      {ui::AX_ROLE_COMBO_BOX, NSAccessibilityComboBoxRole},
      {ui::AX_ROLE_COMPLEMENTARY, NSAccessibilityGroupRole},
      {ui::AX_ROLE_CONTENT_INFO, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DATE, @"AXDateField"},
      {ui::AX_ROLE_DATE_TIME, @"AXDateField"},
      {ui::AX_ROLE_DEFINITION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DESCRIPTION_LIST_DETAIL, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DESCRIPTION_LIST, NSAccessibilityListRole},
      {ui::AX_ROLE_DESCRIPTION_LIST_TERM, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DIALOG, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DETAILS, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DIRECTORY, NSAccessibilityListRole},
      // If Mac supports AXExpandedChanged event with
      // NSAccessibilityDisclosureTriangleRole, We should update
      // AX_ROLE_DISCLOSURE_TRIANGLE mapping to
      // NSAccessibilityDisclosureTriangleRole. http://crbug.com/558324
      {ui::AX_ROLE_DISCLOSURE_TRIANGLE, NSAccessibilityButtonRole},
      {ui::AX_ROLE_DOCUMENT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_EMBEDDED_OBJECT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_FIGCAPTION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_FIGURE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_FOOTER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_FORM, NSAccessibilityGroupRole},
      {ui::AX_ROLE_GENERIC_CONTAINER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_GRID, NSAccessibilityGridRole},
      {ui::AX_ROLE_GROUP, NSAccessibilityGroupRole},
      {ui::AX_ROLE_HEADING, @"AXHeading"},
      {ui::AX_ROLE_IFRAME, NSAccessibilityGroupRole},
      {ui::AX_ROLE_IFRAME_PRESENTATIONAL, NSAccessibilityGroupRole},
      {ui::AX_ROLE_IGNORED, NSAccessibilityUnknownRole},
      {ui::AX_ROLE_IMAGE, NSAccessibilityImageRole},
      {ui::AX_ROLE_IMAGE_MAP, NSAccessibilityGroupRole},
      {ui::AX_ROLE_IMAGE_MAP_LINK, NSAccessibilityLinkRole},
      {ui::AX_ROLE_INPUT_TIME, @"AXTimeField"},
      {ui::AX_ROLE_LABEL_TEXT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_LEGEND, NSAccessibilityGroupRole},
      {ui::AX_ROLE_LINK, NSAccessibilityLinkRole},
      {ui::AX_ROLE_LIST, NSAccessibilityListRole},
      {ui::AX_ROLE_LIST_BOX, NSAccessibilityListRole},
      {ui::AX_ROLE_LIST_BOX_OPTION, NSAccessibilityStaticTextRole},
      {ui::AX_ROLE_LIST_ITEM, NSAccessibilityGroupRole},
      {ui::AX_ROLE_LIST_MARKER, @"AXListMarker"},
      {ui::AX_ROLE_LOG, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MAIN, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MARK, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MARQUEE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MATH, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MENU, NSAccessibilityMenuRole},
      {ui::AX_ROLE_MENU_BAR, NSAccessibilityMenuBarRole},
      {ui::AX_ROLE_MENU_BUTTON, NSAccessibilityButtonRole},
      {ui::AX_ROLE_MENU_ITEM, NSAccessibilityMenuItemRole},
      {ui::AX_ROLE_MENU_ITEM_CHECK_BOX, NSAccessibilityMenuItemRole},
      {ui::AX_ROLE_MENU_ITEM_RADIO, NSAccessibilityMenuItemRole},
      {ui::AX_ROLE_MENU_LIST_OPTION, NSAccessibilityMenuItemRole},
      {ui::AX_ROLE_MENU_LIST_POPUP, NSAccessibilityUnknownRole},
      {ui::AX_ROLE_METER, NSAccessibilityProgressIndicatorRole},
      {ui::AX_ROLE_NAVIGATION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_NONE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_NOTE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_OUTLINE, NSAccessibilityOutlineRole},
      {ui::AX_ROLE_PARAGRAPH, NSAccessibilityGroupRole},
      {ui::AX_ROLE_POP_UP_BUTTON, NSAccessibilityPopUpButtonRole},
      {ui::AX_ROLE_PRE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_PRESENTATIONAL, NSAccessibilityGroupRole},
      {ui::AX_ROLE_PROGRESS_INDICATOR, NSAccessibilityProgressIndicatorRole},
      {ui::AX_ROLE_RADIO_BUTTON, NSAccessibilityRadioButtonRole},
      {ui::AX_ROLE_RADIO_GROUP, NSAccessibilityRadioGroupRole},
      {ui::AX_ROLE_REGION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ROOT_WEB_AREA, @"AXWebArea"},
      {ui::AX_ROLE_ROW, NSAccessibilityRowRole},
      {ui::AX_ROLE_ROW_HEADER, @"AXCell"},
      {ui::AX_ROLE_RULER, NSAccessibilityRulerRole},
      {ui::AX_ROLE_SCROLL_BAR, NSAccessibilityScrollBarRole},
      {ui::AX_ROLE_SEARCH, NSAccessibilityGroupRole},
      {ui::AX_ROLE_SEARCH_BOX, NSAccessibilityTextFieldRole},
      {ui::AX_ROLE_SLIDER, NSAccessibilitySliderRole},
      {ui::AX_ROLE_SLIDER_THUMB, NSAccessibilityValueIndicatorRole},
      {ui::AX_ROLE_SPIN_BUTTON, NSAccessibilityIncrementorRole},
      {ui::AX_ROLE_SPLITTER, NSAccessibilitySplitterRole},
      {ui::AX_ROLE_STATIC_TEXT, NSAccessibilityStaticTextRole},
      {ui::AX_ROLE_STATUS, NSAccessibilityGroupRole},
      {ui::AX_ROLE_SVG_ROOT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_SWITCH, NSAccessibilityCheckBoxRole},
      {ui::AX_ROLE_TAB, NSAccessibilityRadioButtonRole},
      {ui::AX_ROLE_TABLE, NSAccessibilityTableRole},
      {ui::AX_ROLE_TABLE_HEADER_CONTAINER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TAB_LIST, NSAccessibilityTabGroupRole},
      {ui::AX_ROLE_TAB_PANEL, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TERM, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TEXT_FIELD, NSAccessibilityTextFieldRole},
      {ui::AX_ROLE_TIME, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TIMER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TOGGLE_BUTTON, NSAccessibilityCheckBoxRole},
      {ui::AX_ROLE_TOOLBAR, NSAccessibilityToolbarRole},
      {ui::AX_ROLE_TOOLTIP, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TREE, NSAccessibilityOutlineRole},
      {ui::AX_ROLE_TREE_GRID, NSAccessibilityTableRole},
      {ui::AX_ROLE_TREE_ITEM, NSAccessibilityRowRole},
      {ui::AX_ROLE_VIDEO, NSAccessibilityGroupRole},
      {ui::AX_ROLE_WEB_AREA, @"AXWebArea"},
      {ui::AX_ROLE_WINDOW, NSAccessibilityWindowRole},

      // TODO(dtseng): we don't correctly support the attributes for these
      // roles.
      // { ui::AX_ROLE_SCROLL_AREA, NSAccessibilityScrollAreaRole },
  };

  return RoleMap(begin(roles), end(roles));
}

RoleMap BuildSubroleMap() {
  const RoleMap::value_type subroles[] = {
      {ui::AX_ROLE_ALERT, @"AXApplicationAlert"},
      {ui::AX_ROLE_ALERT_DIALOG, @"AXApplicationAlertDialog"},
      {ui::AX_ROLE_APPLICATION, @"AXLandmarkApplication"},
      {ui::AX_ROLE_ARTICLE, @"AXDocumentArticle"},
      {ui::AX_ROLE_BANNER, @"AXLandmarkBanner"},
      {ui::AX_ROLE_COMPLEMENTARY, @"AXLandmarkComplementary"},
      {ui::AX_ROLE_CONTENT_INFO, @"AXLandmarkContentInfo"},
      {ui::AX_ROLE_DEFINITION, @"AXDefinition"},
      {ui::AX_ROLE_DESCRIPTION_LIST_DETAIL, @"AXDefinition"},
      {ui::AX_ROLE_DESCRIPTION_LIST_TERM, @"AXTerm"},
      {ui::AX_ROLE_DIALOG, @"AXApplicationDialog"},
      {ui::AX_ROLE_DOCUMENT, @"AXDocument"},
      {ui::AX_ROLE_FOOTER, @"AXLandmarkContentInfo"},
      {ui::AX_ROLE_FORM, @"AXLandmarkForm"},
      {ui::AX_ROLE_LOG, @"AXApplicationLog"},
      {ui::AX_ROLE_MAIN, @"AXLandmarkMain"},
      {ui::AX_ROLE_MARQUEE, @"AXApplicationMarquee"},
      {ui::AX_ROLE_MATH, @"AXDocumentMath"},
      {ui::AX_ROLE_NAVIGATION, @"AXLandmarkNavigation"},
      {ui::AX_ROLE_NOTE, @"AXDocumentNote"},
      {ui::AX_ROLE_REGION, @"AXDocumentRegion"},
      {ui::AX_ROLE_SEARCH, @"AXLandmarkSearch"},
      {ui::AX_ROLE_SEARCH_BOX, @"AXSearchField"},
      {ui::AX_ROLE_STATUS, @"AXApplicationStatus"},
      {ui::AX_ROLE_SWITCH, @"AXSwitch"},
      {ui::AX_ROLE_TAB_PANEL, @"AXTabPanel"},
      {ui::AX_ROLE_TERM, @"AXTerm"},
      {ui::AX_ROLE_TIMER, @"AXApplicationTimer"},
      {ui::AX_ROLE_TOGGLE_BUTTON, @"AXToggleButton"},
      {ui::AX_ROLE_TOOLTIP, @"AXUserInterfaceTooltip"},
      {ui::AX_ROLE_TREE_ITEM, NSAccessibilityOutlineRowSubrole},
  };

  return RoleMap(begin(subroles), end(subroles));
}

EventMap BuildEventMap() {
  const EventMap::value_type events[] = {
      {ui::AX_EVENT_FOCUS, NSAccessibilityFocusedUIElementChangedNotification},
      {ui::AX_EVENT_TEXT_CHANGED, NSAccessibilityTitleChangedNotification},
      {ui::AX_EVENT_VALUE_CHANGED, NSAccessibilityValueChangedNotification},
      {ui::AX_EVENT_TEXT_SELECTION_CHANGED,
       NSAccessibilitySelectedTextChangedNotification},
      // TODO(patricialor): Add more events.
  };

  return EventMap(begin(events), end(events));
}

ActionList BuildActionList() {
  const ActionList::value_type entries[] = {
      // NSAccessibilityPressAction must come first in this list.
      {ui::AX_ACTION_DO_DEFAULT, NSAccessibilityPressAction},

      {ui::AX_ACTION_DECREMENT, NSAccessibilityDecrementAction},
      {ui::AX_ACTION_INCREMENT, NSAccessibilityIncrementAction},
      {ui::AX_ACTION_SHOW_CONTEXT_MENU, NSAccessibilityShowMenuAction},
  };
  return ActionList(begin(entries), end(entries));
}

const ActionList& GetActionList() {
  CR_DEFINE_STATIC_LOCAL(const ActionList, action_map, (BuildActionList()));
  return action_map;
}

void NotifyMacEvent(AXPlatformNodeCocoa* target, ui::AXEvent event_type) {
  NSAccessibilityPostNotification(
      target, [AXPlatformNodeCocoa nativeNotificationFromAXEvent:event_type]);
}

// Returns true if |action| should be added implicitly for |data|.
bool HasImplicitAction(const ui::AXNodeData& data, ui::AXAction action) {
  return action == ui::AX_ACTION_DO_DEFAULT && ui::IsRoleClickable(data.role);
}

// For roles that show a menu for the default action, ensure "show menu" also
// appears in available actions, but only if that's not already used for a
// context menu. It will be mapped back to the default action when performed.
bool AlsoUseShowMenuActionForDefaultAction(const ui::AXNodeData& data) {
  return HasImplicitAction(data, ui::AX_ACTION_DO_DEFAULT) &&
         !data.HasAction(ui::AX_ACTION_SHOW_CONTEXT_MENU) &&
         data.role == ui::AX_ROLE_POP_UP_BUTTON;
}

}  // namespace

@interface AXPlatformNodeCocoa ()
// Helper function for string attributes that don't require extra processing.
- (NSString*)getStringAttribute:(ui::AXStringAttribute)attribute;
// Returns AXValue, or nil if AXValue isn't an NSString.
- (NSString*)getAXValueAsString;
@end

@implementation AXPlatformNodeCocoa {
  ui::AXPlatformNodeBase* node_;  // Weak. Retains us.
}

@synthesize node = node_;

+ (NSString*)nativeRoleFromAXRole:(ui::AXRole)role {
  CR_DEFINE_STATIC_LOCAL(const RoleMap, role_map, (BuildRoleMap()));
  RoleMap::const_iterator it = role_map.find(role);
  return it != role_map.end() ? it->second : NSAccessibilityUnknownRole;
}

+ (NSString*)nativeSubroleFromAXRole:(ui::AXRole)role {
  CR_DEFINE_STATIC_LOCAL(const RoleMap, subrole_map, (BuildSubroleMap()));
  RoleMap::const_iterator it = subrole_map.find(role);
  return it != subrole_map.end() ? it->second : nil;
}

+ (NSString*)nativeNotificationFromAXEvent:(ui::AXEvent)event {
  CR_DEFINE_STATIC_LOCAL(const EventMap, event_map, (BuildEventMap()));
  EventMap::const_iterator it = event_map.find(event);
  return it != event_map.end() ? it->second : nil;
}

- (instancetype)initWithNode:(ui::AXPlatformNodeBase*)node {
  if ((self = [super init])) {
    node_ = node;
  }
  return self;
}

- (void)detach {
  if (!node_)
    return;
  NSAccessibilityPostNotification(
      self, NSAccessibilityUIElementDestroyedNotification);
  node_ = nil;
}

- (NSRect)boundsInScreen {
  if (!node_)
    return NSZeroRect;
  return gfx::ScreenRectToNSRect(node_->GetBoundsInScreen());
}

- (NSString*)getStringAttribute:(ui::AXStringAttribute)attribute {
  std::string attributeValue;
  if (node_->GetStringAttribute(attribute, &attributeValue))
    return base::SysUTF8ToNSString(attributeValue);
  return nil;
}

- (NSString*)getAXValueAsString {
  id value = [self AXValue];
  return [value isKindOfClass:[NSString class]] ? value : nil;
}

// NSAccessibility informal protocol implementation.

- (BOOL)accessibilityIsIgnored {
  if (!node_)
    return YES;

  return [[self AXRole] isEqualToString:NSAccessibilityUnknownRole] ||
         node_->GetData().HasState(ui::AX_STATE_INVISIBLE);
}

- (id)accessibilityHitTest:(NSPoint)point {
  for (AXPlatformNodeCocoa* child in [self AXChildren]) {
    if (![child accessibilityIsIgnored] &&
        NSPointInRect(point, child.boundsInScreen)) {
      return [child accessibilityHitTest:point];
    }
  }
  return NSAccessibilityUnignoredAncestor(self);
}

- (BOOL)accessibilityNotifiesWhenDestroyed {
  return YES;
}

- (id)accessibilityFocusedUIElement {
  return node_ ? node_->GetDelegate()->GetFocus() : nil;
}

- (NSArray*)accessibilityActionNames {
  if (!node_)
    return @[];

  base::scoped_nsobject<NSMutableArray> axActions(
      [[NSMutableArray alloc] init]);

  const ui::AXNodeData& data = node_->GetData();
  const ActionList& action_list = GetActionList();

  // VoiceOver expects the "press" action to be first. Note that some roles
  // should be given a press action implicitly.
  DCHECK([action_list[0].second isEqualToString:NSAccessibilityPressAction]);
  for (const auto item : action_list) {
    if (data.HasAction(item.first) || HasImplicitAction(data, item.first))
      [axActions addObject:item.second];
  }

  if (AlsoUseShowMenuActionForDefaultAction(data))
    [axActions addObject:NSAccessibilityShowMenuAction];

  return axActions.autorelease();
}

- (void)accessibilityPerformAction:(NSString*)action {
  // Actions are performed asynchronously, so it's always possible for an object
  // to change its mind after previously reporting an action as available.
  if (![[self accessibilityActionNames] containsObject:action])
    return;

  ui::AXActionData data;
  if ([action isEqualToString:NSAccessibilityShowMenuAction] &&
      AlsoUseShowMenuActionForDefaultAction(node_->GetData())) {
    data.action = ui::AX_ACTION_DO_DEFAULT;
  } else {
    for (const ActionList::value_type& entry : GetActionList()) {
      if ([action isEqualToString:entry.second]) {
        data.action = entry.first;
        break;
      }
    }
  }

  // Note ui::AX_ACTIONs which are just overwriting an accessibility attribute
  // are already implemented in -accessibilitySetValue:forAttribute:, so ignore
  // those here.

  if (data.action != ui::AX_ACTION_NONE)
    node_->GetDelegate()->AccessibilityPerformAction(data);
}

- (NSArray*)accessibilityAttributeNames {
  if (!node_)
    return @[];

  // These attributes are required on all accessibility objects.
  NSArray* const kAllRoleAttributes = @[
    NSAccessibilityChildrenAttribute,
    NSAccessibilityParentAttribute,
    NSAccessibilityPositionAttribute,
    NSAccessibilityRoleAttribute,
    NSAccessibilitySizeAttribute,
    NSAccessibilitySubroleAttribute,

    // Title is required for most elements. Cocoa asks for the value even if it
    // is omitted here, but won't present it to accessibility APIs without this.
    NSAccessibilityTitleAttribute,

    // Attributes which are not required, but are general to all roles.
    NSAccessibilityRoleDescriptionAttribute,
    NSAccessibilityEnabledAttribute,
    NSAccessibilityFocusedAttribute,
    NSAccessibilityHelpAttribute,
    NSAccessibilityTopLevelUIElementAttribute,
    NSAccessibilityWindowAttribute,
  ];

  // Attributes required for user-editable controls.
  NSArray* const kValueAttributes = @[ NSAccessibilityValueAttribute ];

  // Attributes required for unprotected textfields and labels.
  NSArray* const kUnprotectedTextAttributes = @[
    NSAccessibilityInsertionPointLineNumberAttribute,
    NSAccessibilityNumberOfCharactersAttribute,
    NSAccessibilitySelectedTextAttribute,
    NSAccessibilitySelectedTextRangeAttribute,
    NSAccessibilityVisibleCharacterRangeAttribute,
  ];

  // Required for all text, including protected textfields.
  NSString* const kTextAttributes = NSAccessibilityPlaceholderValueAttribute;

  base::scoped_nsobject<NSMutableArray> axAttributes(
      [[NSMutableArray alloc] init]);

  [axAttributes addObjectsFromArray:kAllRoleAttributes];

  switch (node_->GetData().role) {
    case ui::AX_ROLE_TEXT_FIELD:
    case ui::AX_ROLE_STATIC_TEXT:
      [axAttributes addObject:kTextAttributes];
      if (!node_->GetData().HasState(ui::AX_STATE_PROTECTED))
        [axAttributes addObjectsFromArray:kUnprotectedTextAttributes];
    // Fallthrough.
    case ui::AX_ROLE_CHECK_BOX:
    case ui::AX_ROLE_COMBO_BOX:
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
    case ui::AX_ROLE_MENU_ITEM_RADIO:
    case ui::AX_ROLE_RADIO_BUTTON:
    case ui::AX_ROLE_SEARCH_BOX:
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_SLIDER_THUMB:
    case ui::AX_ROLE_TOGGLE_BUTTON:
      [axAttributes addObjectsFromArray:kValueAttributes];
      break;
    // TODO(tapted): Add additional attributes based on role.
    default:
      break;
  }
  return axAttributes.autorelease();
}

- (NSArray*)accessibilityParameterizedAttributeNames {
  if (!node_)
    return @[];

  static NSArray* const kSelectableTextAttributes = [@[
    NSAccessibilityLineForIndexParameterizedAttribute,
    NSAccessibilityRangeForLineParameterizedAttribute,
    NSAccessibilityStringForRangeParameterizedAttribute,
    NSAccessibilityRangeForPositionParameterizedAttribute,
    NSAccessibilityRangeForIndexParameterizedAttribute,
    NSAccessibilityBoundsForRangeParameterizedAttribute,
    NSAccessibilityRTFForRangeParameterizedAttribute,
    NSAccessibilityStyleRangeForIndexParameterizedAttribute,
    NSAccessibilityAttributedStringForRangeParameterizedAttribute,
  ] retain];

  switch (node_->GetData().role) {
    case ui::AX_ROLE_TEXT_FIELD:
    case ui::AX_ROLE_STATIC_TEXT:
      return kSelectableTextAttributes;
    default:
      break;
  }
  return nil;
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attributeName {
  if (!node_)
    return NO;

  const int restriction =
      node_->GetData().GetIntAttribute(ui::AX_ATTR_RESTRICTION);
  if (restriction == ui::AX_RESTRICTION_DISABLED)
    return NO;

  // Allow certain attributes to be written via an accessibility client. A
  // writable attribute will only appear as such if the accessibility element
  // has a value set for that attribute.
  if ([attributeName
          isEqualToString:NSAccessibilitySelectedChildrenAttribute] ||
      [attributeName
          isEqualToString:NSAccessibilityVisibleCharacterRangeAttribute]) {
    return NO;
  }

  if ([attributeName isEqualToString:NSAccessibilityValueAttribute]) {
    // Since tabs use the Radio Button role on Mac, the standard way to set
    // them is via the value attribute rather than the selected attribute.
    if (node_->GetData().role == ui::AX_ROLE_TAB)
      return !node_->GetData().HasState(ui::AX_STATE_SELECTED);

    return restriction != ui::AX_RESTRICTION_READ_ONLY;
  }

  // Readonly fields and selected text operations:
  // - Selecting different text via NSAccessibilitySelectedTextRangeAttribute
  //   should work but it does not - see http://crbug.com/692362 .
  // - Changing the actual text contents in the selection via
  //   NSAccessibilitySelectedTextAttribute is prevented, which is correct.
  if ([attributeName isEqualToString:NSAccessibilitySelectedTextAttribute] ||
      [attributeName isEqualToString:NSAccessibilitySelectedTextRangeAttribute])
    return restriction != ui::AX_RESTRICTION_READ_ONLY;

  if ([attributeName isEqualToString:NSAccessibilityFocusedAttribute]) {
    return node_->GetData().HasState(ui::AX_STATE_FOCUSABLE);
  }

  // TODO(patricialor): Add callbacks for updating the above attributes except
  // NSAccessibilityValueAttribute and return YES.
  return NO;
}

- (void)accessibilitySetValue:(id)value forAttribute:(NSString*)attribute {
  if (!node_)
    return;

  ui::AXActionData data;

  // Check for attributes first. Only the |data.action| should be set here - any
  // type-specific information, if needed, should be set below.
  if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    data.action = node_->GetData().role == ui::AX_ROLE_TAB
                      ? ui::AX_ACTION_SET_SELECTION
                      : ui::AX_ACTION_SET_VALUE;
  } else if ([attribute isEqualToString:NSAccessibilitySelectedTextAttribute]) {
    data.action = ui::AX_ACTION_REPLACE_SELECTED_TEXT;
  } else if ([attribute
                 isEqualToString:NSAccessibilitySelectedTextRangeAttribute]) {
    data.action = ui::AX_ACTION_SET_SELECTION;
  } else if ([attribute isEqualToString:NSAccessibilityFocusedAttribute]) {
    if ([value isKindOfClass:[NSNumber class]]) {
      data.action =
          [value boolValue] ? ui::AX_ACTION_FOCUS : ui::AX_ACTION_BLUR;
    }
  }

  // Set type-specific information as necessary for actions set above.
  if ([value isKindOfClass:[NSString class]]) {
    data.value = base::SysNSStringToUTF16(value);
  } else if (data.action == ui::AX_ACTION_SET_SELECTION &&
             [value isKindOfClass:[NSValue class]]) {
    NSRange range = [value rangeValue];
    data.anchor_offset = range.location;
    data.focus_offset = NSMaxRange(range);
  }

  if (data.action != ui::AX_ACTION_NONE)
    node_->GetDelegate()->AccessibilityPerformAction(data);

  // TODO(patricialor): Plumb through all the other writable attributes as
  // specified in accessibilityIsAttributeSettable.
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (!node_)
    return nil;  // Return nil when detached. Even for AXRole.

  SEL selector = NSSelectorFromString(attribute);
  if ([self respondsToSelector:selector])
    return [self performSelector:selector];
  return nil;
}

- (id)accessibilityAttributeValue:(NSString*)attribute
                     forParameter:(id)parameter {
  if (!node_)
    return nil;

  SEL selector = NSSelectorFromString([attribute stringByAppendingString:@":"]);
  if ([self respondsToSelector:selector])
    return [self performSelector:selector withObject:parameter];
  return nil;
}

// NSAccessibility attributes. Order them according to
// NSAccessibilityConstants.h, or see https://crbug.com/678898.

- (NSString*)AXRole {
  if (!node_)
    return nil;

  return [[self class] nativeRoleFromAXRole:node_->GetData().role];
}

- (NSString*)AXRoleDescription {
  switch (node_->GetData().role) {
    case ui::AX_ROLE_TAB:
      // There is no NSAccessibilityTabRole or similar (AXRadioButton is used
      // instead). Do the same as NSTabView and put "tab" in the description.
      return [l10n_util::GetNSStringWithFixup(IDS_ACCNAME_TAB_ROLE_DESCRIPTION)
          lowercaseString];
    case ui::AX_ROLE_DISCLOSURE_TRIANGLE:
      return [l10n_util::GetNSStringWithFixup(
          IDS_ACCNAME_DISCLOSURE_TRIANGLE_ROLE_DESCRIPTION) lowercaseString];
    default:
      break;
  }
  return NSAccessibilityRoleDescription([self AXRole], [self AXSubrole]);
}

- (NSString*)AXSubrole {
  ui::AXRole role = node_->GetData().role;
  switch (role) {
    case ui::AX_ROLE_TEXT_FIELD:
      if (node_->GetData().HasState(ui::AX_STATE_PROTECTED))
        return NSAccessibilitySecureTextFieldSubrole;
      break;
    default:
      break;
  }
  return [AXPlatformNodeCocoa nativeSubroleFromAXRole:role];
}

- (NSString*)AXHelp {
  return [self getStringAttribute:ui::AX_ATTR_DESCRIPTION];
}

- (id)AXValue {
  switch (node_->GetData().role) {
    case ui::AX_ROLE_TAB:
      return [self AXSelected];
    case ui::AX_ROLE_STATIC_TEXT:
      return [self AXTitle];
    default:
      break;
  }
  return [self getStringAttribute:ui::AX_ATTR_VALUE];
}

- (NSNumber*)AXEnabled {
  return @(node_->GetData().GetIntAttribute(ui::AX_ATTR_RESTRICTION) !=
           ui::AX_RESTRICTION_DISABLED);
}

- (NSNumber*)AXFocused {
  if (node_->GetData().HasState(ui::AX_STATE_FOCUSABLE))
    return
        @(node_->GetDelegate()->GetFocus() == node_->GetNativeViewAccessible());
  return @NO;
}

- (id)AXParent {
  if (!node_)
    return nil;
  return NSAccessibilityUnignoredAncestor(node_->GetParent());
}

- (NSArray*)AXChildren {
  if (!node_)
    return @[];

  int count = node_->GetChildCount();
  NSMutableArray* children = [NSMutableArray arrayWithCapacity:count];
  for (int i = 0; i < count; ++i)
    [children addObject:node_->ChildAtIndex(i)];
  return NSAccessibilityUnignoredChildren(children);
}

- (id)AXWindow {
  return node_->GetDelegate()->GetTopLevelWidget();
}

- (id)AXTopLevelUIElement {
  return [self AXWindow];
}

- (NSValue*)AXPosition {
  return [NSValue valueWithPoint:self.boundsInScreen.origin];
}

- (NSValue*)AXSize {
  return [NSValue valueWithSize:self.boundsInScreen.size];
}

- (NSString*)AXTitle {
  return [self getStringAttribute:ui::AX_ATTR_NAME];
}

// Misc attributes.

- (NSNumber*)AXSelected {
  return @(node_->GetData().HasState(ui::AX_STATE_SELECTED));
}

- (NSString*)AXPlaceholderValue {
  return [self getStringAttribute:ui::AX_ATTR_PLACEHOLDER];
}

// Text-specific attributes.

- (NSString*)AXSelectedText {
  NSRange selectedTextRange;
  [[self AXSelectedTextRange] getValue:&selectedTextRange];
  return [[self getAXValueAsString] substringWithRange:selectedTextRange];
}

- (NSValue*)AXSelectedTextRange {
  // Selection might not be supported. Return (NSRange){0,0} in that case.
  int start = 0, end = 0;
  node_->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_START, &start);
  node_->GetIntAttribute(ui::AX_ATTR_TEXT_SEL_END, &end);

  // NSRange cannot represent the direction the text was selected in.
  return [NSValue valueWithRange:{std::min(start, end), abs(end - start)}];
}

- (NSNumber*)AXNumberOfCharacters {
  return @([[self getAXValueAsString] length]);
}

- (NSValue*)AXVisibleCharacterRange {
  return [NSValue valueWithRange:{0, [[self getAXValueAsString] length]}];
}

- (NSNumber*)AXInsertionPointLineNumber {
  // Multiline is not supported on views.
  return @0;
}

// Parameterized text-specific attributes.

- (id)AXLineForIndex:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  // Multiline is not supported on views.
  return @0;
}

- (id)AXRangeForLine:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  DCHECK_EQ(0, [parameter intValue]);
  return [NSValue valueWithRange:{0, [[self getAXValueAsString] length]}];
}

- (id)AXStringForRange:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  return [[self getAXValueAsString] substringWithRange:[parameter rangeValue]];
}

- (id)AXRangeForPosition:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  // TODO(tapted): Hit-test [parameter pointValue] and return an NSRange.
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXRangeForIndex:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXBoundsForRange:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  // TODO(tapted): Provide an accessor on AXPlatformNodeDelegate to obtain this
  // from ui::TextInputClient::GetCompositionCharacterBounds().
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXRTFForRange:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXStyleRangeForIndex:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSNumber class]]);
  NOTIMPLEMENTED();
  return nil;
}

- (id)AXAttributedStringForRange:(id)parameter {
  DCHECK([parameter isKindOfClass:[NSValue class]]);
  base::scoped_nsobject<NSAttributedString> attributedString(
      [[NSAttributedString alloc]
          initWithString:[self AXStringForRange:parameter]]);
  // TODO(tapted): views::WordLookupClient has a way to obtain the actual
  // decorations, and BridgedContentView has a conversion function that creates
  // an NSAttributedString. Refactor things so they can be used here.
  return attributedString.autorelease();
}

@end

namespace ui {

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeBase* node = new AXPlatformNodeMac();
  node->Init(delegate);
  return node;
}

// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  if ([accessible isKindOfClass:[AXPlatformNodeCocoa class]])
    return [accessible node];
  return nullptr;
}

AXPlatformNodeMac::AXPlatformNodeMac() {
}

AXPlatformNodeMac::~AXPlatformNodeMac() {
}

void AXPlatformNodeMac::Destroy() {
  if (native_node_)
    [native_node_ detach];
  AXPlatformNodeBase::Destroy();
}

gfx::NativeViewAccessible AXPlatformNodeMac::GetNativeViewAccessible() {
  if (!native_node_)
    native_node_.reset([[AXPlatformNodeCocoa alloc] initWithNode:this]);
  return native_node_.get();
}

void AXPlatformNodeMac::NotifyAccessibilityEvent(ui::AXEvent event_type) {
  GetNativeViewAccessible();
  // Add mappings between ui::AXEvent and NSAccessibility notifications using
  // the EventMap above. This switch contains exceptions to those mappings.
  switch (event_type) {
    case ui::AX_EVENT_TEXT_CHANGED:
      // If the view is a user-editable textfield, this should change the value.
      if (GetData().role == ui::AX_ROLE_TEXT_FIELD) {
        NotifyMacEvent(native_node_, ui::AX_EVENT_VALUE_CHANGED);
        return;
      }
      break;
    default:
      break;
  }
  NotifyMacEvent(native_node_, event_type);
}

int AXPlatformNodeMac::GetIndexInParent() {
  // TODO(dmazzoni): implement this.  http://crbug.com/396137
  return -1;
}

}  // namespace ui
