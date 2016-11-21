// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_COCOA_TOUCH_BAR_FORWARD_DECLARATIONS_H_
#define UI_BASE_COCOA_TOUCH_BAR_FORWARD_DECLARATIONS_H_

// Once Chrome is built with the 10.12.1 SDK, the NSTouchBar… classes can be
// referred to normally (instead of with NSClassFromString(@"…")), and this
// file can be deleted.

#import <Foundation/Foundation.h>

#if !defined(MAC_OS_X_VERSION_10_12_1)
#define __NSi_10_12_1 introduced = 10.12.1

NS_ASSUME_NONNULL_BEGIN

@class NSTouchBar, NSTouchBarItem;
@protocol NSTouchBarDelegate;

typedef float NSTouchBarItemPriority;
static const NSTouchBarItemPriority NSTouchBarItemPriorityHigh = 1000;
static const NSTouchBarItemPriority NSTouchBarItemPriorityNormal = 0;
static const NSTouchBarItemPriority NSTouchBarItemPriorityLow = -1000;

typedef NSString* NSTouchBarItemIdentifier;
typedef NSString* NSTouchBarCustomizationIdentifier;

NS_CLASS_AVAILABLE_MAC(10_12_1)
@interface NSTouchBar : NSObject<NSCoding>

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder*)aDecoder
    NS_DESIGNATED_INITIALIZER;

@property(copy, nullable)
    NSTouchBarCustomizationIdentifier customizationIdentifier;
@property(copy) NSArray* customizationAllowedItemIdentifiers;
@property(copy) NSArray* customizationRequiredItemIdentifiers;
@property(copy) NSArray* defaultItemIdentifiers;
@property(copy, readonly) NSArray* itemIdentifiers;
@property(copy, nullable) NSTouchBarItemIdentifier principalItemIdentifier;
@property(copy) NSSet* templateItems;
@property(nullable, weak) id<NSTouchBarDelegate> delegate;

- (nullable __kindof NSTouchBarItem*)itemForIdentifier:
    (NSTouchBarItemIdentifier)identifier;

@property(readonly, getter=isVisible) BOOL visible;

@end

NS_CLASS_AVAILABLE_MAC(10_12_1)
@interface NSTouchBarItem : NSObject<NSCoding>

- (instancetype)initWithIdentifier:(NSTouchBarItemIdentifier)identifier
    NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder*)coder
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(readonly, copy) NSTouchBarItemIdentifier identifier;
@property NSTouchBarItemPriority visibilityPriority;
@property(readonly, nullable) NSView* view;
@property(readonly, nullable) NSViewController* viewController;
@property(readonly, copy) NSString* customizationLabel;
@property(readonly, getter=isVisible) BOOL visible;

@end

NS_CLASS_AVAILABLE_MAC(10_12_1)
@interface NSGroupTouchBarItem : NSTouchBarItem

+ (NSGroupTouchBarItem*)groupItemWithIdentifier:
                            (NSTouchBarItemIdentifier)identifier
                                          items:(NSArray*)items;

@property(strong) NSTouchBar* groupTouchBar;
@property(readwrite, copy, null_resettable) NSString* customizationLabel;

@end

NS_CLASS_AVAILABLE_MAC(10_12_1)
@interface NSCustomTouchBarItem : NSTouchBarItem

@property(readwrite, strong) __kindof NSView* view;
@property(readwrite, strong, nullable)
    __kindof NSViewController* viewController;
@property(readwrite, copy, null_resettable) NSString* customizationLabel;

@end

@protocol NSTouchBarDelegate<NSObject>

@optional
- (nullable NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
               makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
    NS_AVAILABLE_MAC(10_12_1);
@end

NS_ASSUME_NONNULL_END

#endif  // MAC_OS_X_VERSION_10_12_1

#endif  // UI_BASE_COCOA_TOUCH_BAR_FORWARD_DECLARATIONS_H_
