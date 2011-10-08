// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#import <AppKit/AppKit.h>
#include <set>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "remoting/host/chromoting_host.h"
#import "third_party/GTM/AppKit/GTMCarbonEvent.h"

// Esc Key Code is 53.
// http://boredzo.org/blog/wp-content/uploads/2007/05/IMTx-virtual-keycodes.pdf
static const NSUInteger kEscKeyCode = 53;

namespace {
typedef std::set<remoting::ChromotingHost*> Hosts;
}

@interface HotKeyMonitor : NSObject {
 @private
  GTMCarbonHotKey* hot_key_;
  base::Lock hosts_lock_;
  Hosts hosts_;
}

// Called when the hotKey is hit.
- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey;

// Must be called when the HotKeyMonitor is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

// Called to add a host to the list of those to be Shutdown() when the hotkey
// is pressed.
- (void)addHost:(remoting::ChromotingHost*)host;

// Called to remove a host. Returns true if it was the last host being
// monitored, in which case the object should be destroyed.
- (bool)removeHost:(remoting::ChromotingHost*)host;

@end

@implementation HotKeyMonitor

- (id)init {
  if ((self = [super init])) {
    GTMCarbonEventDispatcherHandler* handler =
        [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
    hot_key_ = [handler registerHotKey:kEscKeyCode
                             modifiers:(NSAlternateKeyMask | NSControlKeyMask)
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
  base::AutoLock lock(hosts_lock_);
  for (Hosts::const_iterator i = hosts_.begin(); i != hosts_.end(); ++i) {
    (*i)->Shutdown(NULL);
  }
}

- (void)invalidate {
  GTMCarbonEventDispatcherHandler* handler =
      [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
  [handler unregisterHotKey:hot_key_];
}

- (void)addHost:(remoting::ChromotingHost*)host {
  base::AutoLock lock(hosts_lock_);
  hosts_.insert(host);
}

- (bool)removeHost:(remoting::ChromotingHost*)host {
  base::AutoLock lock(hosts_lock_);
  hosts_.erase(host);
  return hosts_.empty();
}

@end

namespace {

class LocalInputMonitorMac : public remoting::LocalInputMonitor {
 public:
  LocalInputMonitorMac() : host_(NULL) {}
  virtual ~LocalInputMonitorMac();
  virtual void Start(remoting::ChromotingHost* host) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  remoting::ChromotingHost* host_;
  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorMac);
};

base::LazyInstance<base::Lock> monitor_lock(base::LINKER_INITIALIZED);
HotKeyMonitor* hot_key_monitor = NULL;

}  // namespace

LocalInputMonitorMac::~LocalInputMonitorMac() {
  Stop();
}

void LocalInputMonitorMac::Start(remoting::ChromotingHost* host) {
  base::AutoLock lock(monitor_lock.Get());
  if (!hot_key_monitor)
    hot_key_monitor = [[HotKeyMonitor alloc] init];
  CHECK(hot_key_monitor);
  [hot_key_monitor addHost:host];
  host_ = host;
}

void LocalInputMonitorMac::Stop() {
  base::AutoLock lock(monitor_lock.Get());
  if ([hot_key_monitor removeHost:host_]) {
    [hot_key_monitor invalidate];
    [hot_key_monitor release];
    hot_key_monitor = nil;
  }
}

remoting::LocalInputMonitor* remoting::LocalInputMonitor::Create() {
  return new LocalInputMonitorMac;
}
