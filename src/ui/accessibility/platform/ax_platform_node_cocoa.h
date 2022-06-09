// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_COCOA_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_COCOA_H_

#include <memory>

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/accessibility/ax_export.h"

namespace ui {

class AXPlatformNodeBase;

struct AXAnnouncementSpec {
  AXAnnouncementSpec();
  ~AXAnnouncementSpec();

  base::scoped_nsobject<NSString> announcement;
  base::scoped_nsobject<NSWindow> window;
  bool is_polite;
};

}  // namespace ui

AX_EXPORT
@interface AXPlatformNodeCocoa : NSAccessibilityElement <NSAccessibility>

// Determines if this object is alive, i.e. it hasn't been detached.
- (BOOL)instanceActive;

// Maps AX roles to native roles. Returns NSAccessibilityUnknownRole if not
// found.
+ (NSString*)nativeRoleFromAXRole:(ax::mojom::Role)role;

// Maps AX roles to native subroles. Returns nil if not found.
+ (NSString*)nativeSubroleFromAXRole:(ax::mojom::Role)role;

// Maps AX events to native notifications. Returns nil if not found.
+ (NSString*)nativeNotificationFromAXEvent:(ax::mojom::Event)event;

- (instancetype)initWithNode:(ui::AXPlatformNodeBase*)node;
- (void)detach;

@property(nonatomic, readonly) NSRect boundsInScreen;
@property(nonatomic, readonly) ui::AXPlatformNodeBase* node;

// Returns the data necessary to queue an NSAccessibility announcement if
// |eventType| should be announced, or nullptr otherwise.
- (std::unique_ptr<ui::AXAnnouncementSpec>)announcementForEvent:
    (ax::mojom::Event)eventType;

// Ask the system to announce |announcementText|. This is debounced to happen
// at most every |kLiveRegionDebounceMillis| per node, with only the most
// recent announcement text read, to account for situations with multiple
// notifications happening one after another (for example, results for
// find-in-page updating rapidly as they come in from subframes).
- (void)scheduleLiveRegionAnnouncement:
    (std::unique_ptr<ui::AXAnnouncementSpec>)announcement;

- (id)AXWindow;

@end

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_COCOA_H_
