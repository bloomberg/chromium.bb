// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile_view.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_gesture_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_menu_provider.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ContentSuggestionsMostVisitedTileView ()

// Command handler for the accessibility custom actions.
@property(nonatomic, weak) id<ContentSuggestionsGestureCommands> commandHandler;

// Whether the incognito action should be available.
@property(nonatomic, assign) BOOL incognitoAvailable;

@end

@implementation ContentSuggestionsMostVisitedTileView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _faviconView = [[FaviconView alloc] init];
    _faviconView.font = [UIFont systemFontOfSize:22];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_faviconView.heightAnchor constraintEqualToConstant:32],
      [_faviconView.widthAnchor
          constraintEqualToAnchor:_faviconView.heightAnchor],
    ]];

    [self.imageContainerView addSubview:_faviconView];
    AddSameConstraints(self.imageContainerView, _faviconView);
  }
  return self;
}

- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedItem*)config {
  self = [self initWithFrame:CGRectZero];
  if (self) {
    self.titleLabel.text = config.title;
    self.accessibilityLabel = config.title;
    _incognitoAvailable = config.incognitoAvailable;
    [_faviconView configureWithAttributes:config.attributes];
    _commandHandler = config.commandHandler;
    self.isAccessibilityElement = YES;
    self.accessibilityCustomActions = [self customActions];
    _config = config;
    [self addInteraction:[[UIContextMenuInteraction alloc]
                             initWithDelegate:self]];
  }
  return self;
}

#pragma mark - UIContextMenuInteractionDelegate

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  return [self.menuProvider contextMenuConfigurationForItem:self.config
                                                   fromView:self];
}

#pragma mark - AccessibilityCustomAction

// Custom action for a cell configured with this item.
- (NSArray<UIAccessibilityCustomAction*>*)customActions {
  UIAccessibilityCustomAction* openInNewTab =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                target:self
              selector:@selector(openInNewTab)];
  UIAccessibilityCustomAction* removeMostVisited =
      [[UIAccessibilityCustomAction alloc]
          initWithName:l10n_util::GetNSString(
                           IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)
                target:self
              selector:@selector(removeMostVisited)];

  NSMutableArray* actions =
      [NSMutableArray arrayWithObjects:openInNewTab, removeMostVisited, nil];
  if (self.incognitoAvailable) {
    UIAccessibilityCustomAction* openInNewIncognitoTab =
        [[UIAccessibilityCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWINCOGNITOTAB)
                  target:self
                selector:@selector(openInNewIncognitoTab)];
    [actions addObject:openInNewIncognitoTab];
  }
  return actions;
}

// Target for custom action.
- (BOOL)openInNewTab {
  DCHECK(self.commandHandler);
  [self.commandHandler openNewTabWithMostVisitedItem:self.config incognito:NO];
  return YES;
}

// Target for custom action.
- (BOOL)openInNewIncognitoTab {
  DCHECK(self.commandHandler);
  [self.commandHandler openNewTabWithMostVisitedItem:self.config incognito:YES];
  return YES;
}

// Target for custom action.
- (BOOL)removeMostVisited {
  DCHECK(self.commandHandler);
  [self.commandHandler removeMostVisited:self.config];
  return YES;
}

@end
