// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#import <AppKit/AppKit.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/chromoting_host.h"
#import "third_party/GTM/AppKit/GTMCarbonEvent.h"

// Esc Key Code is 53.
// http://boredzo.org/blog/wp-content/uploads/2007/05/IMTx-virtual-keycodes.pdf
static const NSUInteger kEscKeyCode = 53;

@interface HotKeyMonitor : NSObject {
 @private
  GTMCarbonHotKey* hot_key_;
  remoting::ChromotingHost* host_;
}

// Called when the hotKey is hit.
- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey;

// Must be called when the HotKeyMonitor is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

@end

@implementation HotKeyMonitor

- (id)initWithHost:(remoting::ChromotingHost*)host {
  if ((self = [super init])) {
    host_ = host;
    GTMCarbonEventDispatcherHandler* handler =
        [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
    hot_key_ = [handler registerHotKey:kEscKeyCode
                             modifiers:NSShiftKeyMask
                                target:self
                                action:@selector(hotKeyHit:)
                              userInfo:nil
                           whenPressed:YES];
    if (!hot_key_) {
      [self release];
      return nil;
    }
  }
  return self;
}

- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey {
  host_->Shutdown(NULL);
}

- (void)invalidate {
  GTMCarbonEventDispatcherHandler* handler =
      [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
  [handler unregisterHotKey:hot_key_];
}

@end

namespace {

class LocalInputMonitorMac : public remoting::LocalInputMonitor {
 public:
  LocalInputMonitorMac() : hot_key_monitor_(NULL) {}
  virtual ~LocalInputMonitorMac();
  virtual void Start(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  HotKeyMonitor* hot_key_monitor_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorMac);
};

}  // namespace

LocalInputMonitorMac::~LocalInputMonitorMac() {
  Stop();
}

void LocalInputMonitorMac::Start(remoting::ChromotingHost* host) {
  CHECK(!hot_key_monitor_);
  hot_key_monitor_ = [[HotKeyMonitor alloc] initWithHost:host];
  CHECK(hot_key_monitor_);
}

void LocalInputMonitorMac::Stop() {
  [hot_key_monitor_ invalidate];
  [hot_key_monitor_ release];
  hot_key_monitor_ = nil;
}

remoting::LocalInputMonitor* remoting::LocalInputMonitor::Create() {
  return new LocalInputMonitorMac;
}
