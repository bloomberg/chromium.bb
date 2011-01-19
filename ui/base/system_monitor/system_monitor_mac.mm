// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/system_monitor/system_monitor.h"

#import <AppKit/AppKit.h>

@interface SystemMonitorBridge : NSObject {
 @private
  ui::SystemMonitor* systemMonitor_;  // weak
}

- (id)initWithSystemMonitor:(ui::SystemMonitor*)monitor;
- (void)computerDidSleep:(NSNotification*)notification;
- (void)computerDidWake:(NSNotification*)notification;

@end

@implementation SystemMonitorBridge

- (id)initWithSystemMonitor:(ui::SystemMonitor*)monitor {
  self = [super init];
  if (self) {
    systemMonitor_ = monitor;

    // See QA1340
    // <http://developer.apple.com/library/mac/#qa/qa2004/qa1340.html> for more
    // details.
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(computerDidSleep:)
               name:NSWorkspaceWillSleepNotification
             object:nil];
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(computerDidWake:)
               name:NSWorkspaceDidWakeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      removeObserver:self];
  [super dealloc];
}

- (void)computerDidSleep:(NSNotification*)notification {
  systemMonitor_->ProcessPowerMessage(ui::SystemMonitor::SUSPEND_EVENT);
}

- (void)computerDidWake:(NSNotification*)notification {
  systemMonitor_->ProcessPowerMessage(ui::SystemMonitor::RESUME_EVENT);
}

@end

void ui::SystemMonitor::PlatformInit() {
  system_monitor_bridge_ =
      [[SystemMonitorBridge alloc] initWithSystemMonitor:this];
}

void ui::SystemMonitor::PlatformDestroy() {
  [system_monitor_bridge_ release];
}
