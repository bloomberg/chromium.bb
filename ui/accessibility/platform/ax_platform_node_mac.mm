// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/accessibility/platform/ax_platform_node_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#import "ui/accessibility/platform/ax_platform_node_delegate.h"
#import "ui/gfx/mac/coordinate_conversion.h"

namespace {

struct MapEntry {
  ui::AXRole value;
  NSString* nativeValue;
};

typedef std::map<ui::AXRole, NSString*> RoleMap;

RoleMap BuildRoleMap() {
  const MapEntry roles[] = {
      {ui::AX_ROLE_ALERT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ALERT_DIALOG, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ANNOTATION, NSAccessibilityUnknownRole},
      {ui::AX_ROLE_APPLICATION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ARTICLE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_BANNER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_BROWSER, NSAccessibilityBrowserRole},
      {ui::AX_ROLE_BUSY_INDICATOR, NSAccessibilityBusyIndicatorRole},
      {ui::AX_ROLE_BUTTON, NSAccessibilityButtonRole},
      {ui::AX_ROLE_CANVAS, NSAccessibilityImageRole},
      {ui::AX_ROLE_CELL, @"AXCell"},
      {ui::AX_ROLE_CHECK_BOX, NSAccessibilityCheckBoxRole},
      {ui::AX_ROLE_COLOR_WELL, NSAccessibilityColorWellRole},
      {ui::AX_ROLE_COLUMN, NSAccessibilityColumnRole},
      {ui::AX_ROLE_COLUMN_HEADER, @"AXCell"},
      {ui::AX_ROLE_COMBO_BOX, NSAccessibilityComboBoxRole},
      {ui::AX_ROLE_COMPLEMENTARY, NSAccessibilityGroupRole},
      {ui::AX_ROLE_CONTENT_INFO, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DEFINITION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DESCRIPTION_LIST_DETAIL, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DESCRIPTION_LIST_TERM, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DIALOG, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DIRECTORY, NSAccessibilityListRole},
      {ui::AX_ROLE_DISCLOSURE_TRIANGLE, NSAccessibilityDisclosureTriangleRole},
      {ui::AX_ROLE_DIV, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DOCUMENT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_DRAWER, NSAccessibilityDrawerRole},
      {ui::AX_ROLE_EDITABLE_TEXT, NSAccessibilityTextFieldRole},
      {ui::AX_ROLE_FOOTER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_FORM, NSAccessibilityGroupRole},
      {ui::AX_ROLE_GRID, NSAccessibilityGridRole},
      {ui::AX_ROLE_GROUP, NSAccessibilityGroupRole},
      {ui::AX_ROLE_GROW_AREA, NSAccessibilityGrowAreaRole},
      {ui::AX_ROLE_HEADING, @"AXHeading"},
      {ui::AX_ROLE_HELP_TAG, NSAccessibilityHelpTagRole},
      {ui::AX_ROLE_HORIZONTAL_RULE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_IFRAME, NSAccessibilityGroupRole},
      {ui::AX_ROLE_IGNORED, NSAccessibilityUnknownRole},
      {ui::AX_ROLE_IMAGE, NSAccessibilityImageRole},
      {ui::AX_ROLE_IMAGE_MAP, NSAccessibilityGroupRole},
      {ui::AX_ROLE_IMAGE_MAP_LINK, NSAccessibilityLinkRole},
      {ui::AX_ROLE_INCREMENTOR, NSAccessibilityIncrementorRole},
      {ui::AX_ROLE_LABEL_TEXT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_LINK, NSAccessibilityLinkRole},
      {ui::AX_ROLE_LIST, NSAccessibilityListRole},
      {ui::AX_ROLE_LIST_BOX, NSAccessibilityListRole},
      {ui::AX_ROLE_LIST_BOX_OPTION, NSAccessibilityStaticTextRole},
      {ui::AX_ROLE_LIST_ITEM, NSAccessibilityGroupRole},
      {ui::AX_ROLE_LIST_MARKER, @"AXListMarker"},
      {ui::AX_ROLE_LOG, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MAIN, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MARQUEE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MATH, NSAccessibilityGroupRole},
      {ui::AX_ROLE_MATTE, NSAccessibilityMatteRole},
      {ui::AX_ROLE_MENU, NSAccessibilityMenuRole},
      {ui::AX_ROLE_MENU_BAR, NSAccessibilityMenuBarRole},
      {ui::AX_ROLE_MENU_BUTTON, NSAccessibilityButtonRole},
      {ui::AX_ROLE_MENU_ITEM, NSAccessibilityMenuItemRole},
      {ui::AX_ROLE_MENU_LIST_OPTION, NSAccessibilityMenuItemRole},
      {ui::AX_ROLE_MENU_LIST_POPUP, NSAccessibilityUnknownRole},
      {ui::AX_ROLE_NAVIGATION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_NOTE, NSAccessibilityGroupRole},
      {ui::AX_ROLE_OUTLINE, NSAccessibilityOutlineRole},
      {ui::AX_ROLE_PARAGRAPH, NSAccessibilityGroupRole},
      {ui::AX_ROLE_POP_UP_BUTTON, NSAccessibilityPopUpButtonRole},
      {ui::AX_ROLE_PRESENTATIONAL, NSAccessibilityGroupRole},
      {ui::AX_ROLE_PROGRESS_INDICATOR, NSAccessibilityProgressIndicatorRole},
      {ui::AX_ROLE_RADIO_BUTTON, NSAccessibilityRadioButtonRole},
      {ui::AX_ROLE_RADIO_GROUP, NSAccessibilityRadioGroupRole},
      {ui::AX_ROLE_REGION, NSAccessibilityGroupRole},
      {ui::AX_ROLE_ROOT_WEB_AREA, @"AXWebArea"},
      {ui::AX_ROLE_ROW, NSAccessibilityRowRole},
      {ui::AX_ROLE_ROW_HEADER, @"AXCell"},
      {ui::AX_ROLE_RULER, NSAccessibilityRulerRole},
      {ui::AX_ROLE_RULER_MARKER, NSAccessibilityRulerMarkerRole},
      {ui::AX_ROLE_SCROLL_BAR, NSAccessibilityScrollBarRole},
      {ui::AX_ROLE_SEARCH, NSAccessibilityGroupRole},
      {ui::AX_ROLE_SHEET, NSAccessibilitySheetRole},
      {ui::AX_ROLE_SLIDER, NSAccessibilitySliderRole},
      {ui::AX_ROLE_SLIDER_THUMB, NSAccessibilityValueIndicatorRole},
      {ui::AX_ROLE_SPIN_BUTTON, NSAccessibilitySliderRole},
      {ui::AX_ROLE_SPLITTER, NSAccessibilitySplitterRole},
      {ui::AX_ROLE_SPLIT_GROUP, NSAccessibilitySplitGroupRole},
      {ui::AX_ROLE_STATIC_TEXT, NSAccessibilityStaticTextRole},
      {ui::AX_ROLE_STATUS, NSAccessibilityGroupRole},
      {ui::AX_ROLE_SVG_ROOT, NSAccessibilityGroupRole},
      {ui::AX_ROLE_SYSTEM_WIDE, NSAccessibilityUnknownRole},
      {ui::AX_ROLE_TAB, NSAccessibilityRadioButtonRole},
      {ui::AX_ROLE_TABLE, NSAccessibilityTableRole},
      {ui::AX_ROLE_TABLE_HEADER_CONTAINER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TAB_LIST, NSAccessibilityTabGroupRole},
      {ui::AX_ROLE_TAB_PANEL, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TEXT_AREA, NSAccessibilityTextAreaRole},
      {ui::AX_ROLE_TEXT_FIELD, NSAccessibilityTextFieldRole},
      {ui::AX_ROLE_TIMER, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TOGGLE_BUTTON, NSAccessibilityCheckBoxRole},
      {ui::AX_ROLE_TOOLBAR, NSAccessibilityToolbarRole},
      {ui::AX_ROLE_TOOLTIP, NSAccessibilityGroupRole},
      {ui::AX_ROLE_TREE, NSAccessibilityOutlineRole},
      {ui::AX_ROLE_TREE_GRID, NSAccessibilityTableRole},
      {ui::AX_ROLE_TREE_ITEM, NSAccessibilityRowRole},
      {ui::AX_ROLE_VALUE_INDICATOR, NSAccessibilityValueIndicatorRole},
      {ui::AX_ROLE_WEB_AREA, @"AXWebArea"},
      {ui::AX_ROLE_WINDOW, NSAccessibilityWindowRole},

      // TODO(dtseng): we don't correctly support the attributes for these
      // roles.
      // { ui::AX_ROLE_SCROLL_AREA, NSAccessibilityScrollAreaRole },
  };

  RoleMap role_map;
  for (size_t i = 0; i < arraysize(roles); ++i)
    role_map[roles[i].value] = roles[i].nativeValue;
  return role_map;
}

RoleMap BuildSubroleMap() {
  const MapEntry subroles[] = {
      {ui::AX_ROLE_ALERT, @"AXApplicationAlert"},
      {ui::AX_ROLE_ALERT_DIALOG, @"AXApplicationAlertDialog"},
      {ui::AX_ROLE_ARTICLE, @"AXDocumentArticle"},
      {ui::AX_ROLE_DEFINITION, @"AXDefinition"},
      {ui::AX_ROLE_DESCRIPTION_LIST_DETAIL, @"AXDescription"},
      {ui::AX_ROLE_DESCRIPTION_LIST_TERM, @"AXTerm"},
      {ui::AX_ROLE_DIALOG, @"AXApplicationDialog"},
      {ui::AX_ROLE_DOCUMENT, @"AXDocument"},
      {ui::AX_ROLE_FOOTER, @"AXLandmarkContentInfo"},
      {ui::AX_ROLE_APPLICATION, @"AXLandmarkApplication"},
      {ui::AX_ROLE_BANNER, @"AXLandmarkBanner"},
      {ui::AX_ROLE_COMPLEMENTARY, @"AXLandmarkComplementary"},
      {ui::AX_ROLE_CONTENT_INFO, @"AXLandmarkContentInfo"},
      {ui::AX_ROLE_MAIN, @"AXLandmarkMain"},
      {ui::AX_ROLE_NAVIGATION, @"AXLandmarkNavigation"},
      {ui::AX_ROLE_SEARCH, @"AXLandmarkSearch"},
      {ui::AX_ROLE_LOG, @"AXApplicationLog"},
      {ui::AX_ROLE_MARQUEE, @"AXApplicationMarquee"},
      {ui::AX_ROLE_MATH, @"AXDocumentMath"},
      {ui::AX_ROLE_NOTE, @"AXDocumentNote"},
      {ui::AX_ROLE_REGION, @"AXDocumentRegion"},
      {ui::AX_ROLE_STATUS, @"AXApplicationStatus"},
      {ui::AX_ROLE_TAB_PANEL, @"AXTabPanel"},
      {ui::AX_ROLE_TIMER, @"AXApplicationTimer"},
      {ui::AX_ROLE_TOGGLE_BUTTON, @"AXToggleButton"},
      {ui::AX_ROLE_TOOLTIP, @"AXUserInterfaceTooltip"},
      {ui::AX_ROLE_TREE_ITEM, NSAccessibilityOutlineRowSubrole},
  };

  RoleMap subrole_map;
  for (size_t i = 0; i < arraysize(subroles); ++i)
    subrole_map[subroles[i].value] = subroles[i].nativeValue;
  return subrole_map;
}

}  // namespace

@implementation AXPlatformNodeCocoa

// A mapping of AX roles to native roles.
+ (NSString*)nativeRoleFromAXRole:(ui::AXRole)role {
  CR_DEFINE_STATIC_LOCAL(RoleMap, role_map, (BuildRoleMap()));
  RoleMap::iterator it = role_map.find(role);
  return it != role_map.end() ? it->second : NSAccessibilityUnknownRole;
}

// A mapping of AX roles to native subroles.
+ (NSString*)nativeSubroleFromAXRole:(ui::AXRole)role {
  CR_DEFINE_STATIC_LOCAL(RoleMap, subrole_map, (BuildSubroleMap()));
  RoleMap::iterator it = subrole_map.find(role);
  return it != subrole_map.end() ? it->second : nil;
}

- (instancetype)initWithNode:(ui::AXPlatformNodeBase*)node {
  if ((self = [super init])) {
    node_ = node;
  }
  return self;
}

- (void)detach {
  node_ = nil;
}

- (NSRect)boundsInScreen {
  if (!node_)
    return NSZeroRect;
  return gfx::ScreenRectToNSRect(node_->GetBoundsInScreen());
}

- (NSArray*)AXChildren {
  if (!node_)
    return nil;
  int count = node_->GetChildCount();
  NSMutableArray* children = [NSMutableArray arrayWithCapacity:count];
  for (int i = 0; i < count; ++i)
    [children addObject:node_->ChildAtIndex(i)];
  return NSAccessibilityUnignoredChildren(children);
}

- (id)AXParent {
  if (!node_)
    return nil;
  return NSAccessibilityUnignoredAncestor(node_->GetParent());
}

- (NSValue*)AXPosition {
  return [NSValue valueWithPoint:self.boundsInScreen.origin];
}

- (NSString*)AXRole {
  if (!node_)
    return nil;
  return [[self class] nativeRoleFromAXRole:node_->GetRole()];
}

- (NSValue*)AXSize {
  return [NSValue valueWithSize:self.boundsInScreen.size];
}

// NSAccessibility informal protocol implementation.

- (BOOL)accessibilityIsIgnored {
  return [[self AXRole] isEqualToString:NSAccessibilityUnknownRole];
}

- (id)accessibilityHitTest:(NSPoint)point {
  for (AXPlatformNodeCocoa* child in [self AXChildren]) {
    if (NSPointInRect(point, child.boundsInScreen))
      return [child accessibilityHitTest:point];
  }
  return NSAccessibilityUnignoredAncestor(self);
}

- (NSArray*)accessibilityActionNames {
  return nil;
}

- (NSArray*)accessibilityAttributeNames {
  return @[
    NSAccessibilityChildrenAttribute,
    NSAccessibilityParentAttribute,
    NSAccessibilityPositionAttribute,
    NSAccessibilityRoleAttribute,
    NSAccessibilitySizeAttribute,
  ];
}

- (BOOL)accessibilityIsAttributeSettable:(NSString*)attribute {
  return NO;
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  SEL selector = NSSelectorFromString(attribute);
  if ([self respondsToSelector:selector])
    return [self performSelector:selector];
  return nil;
}

@end

namespace ui {

// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  AXPlatformNodeBase* node = new AXPlatformNodeMac();
  node->Init(delegate);
  return node;
}

AXPlatformNodeMac::AXPlatformNodeMac() {
}

AXPlatformNodeMac::~AXPlatformNodeMac() {
}

void AXPlatformNodeMac::Destroy() {
  if (native_node_)
    [native_node_ detach];
  delegate_ = NULL;
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeMac::GetNativeViewAccessible() {
  if (!native_node_)
    native_node_.reset([[AXPlatformNodeCocoa alloc] initWithNode:this]);
  return native_node_.get();
}

}  // namespace ui
