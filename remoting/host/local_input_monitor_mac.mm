// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#import <AppKit/AppKit.h>
#include <set>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "remoting/host/chromoting_host.h"
#import "third_party/GTM/AppKit/GTMCarbonEvent.h"

// Esc Key Code is 53.
// http://boredzo.org/blog/wp-content/uploads/2007/05/IMTx-virtual-keycodes.pdf
static const NSUInteger kEscKeyCode = 53;

namespace {
typedef std::set<scoped_refptr<remoting::ChromotingHost> > Hosts;
}

@interface LocalInputMonitorImpl : NSObject {
 @private
  GTMCarbonHotKey* hotKey_;
  CFRunLoopSourceRef mouseRunLoopSource_;
  base::mac::ScopedCFTypeRef<CFMachPortRef> mouseMachPort_;
  base::Lock hostsLock_;
  Hosts hosts_;
}

// Called when the hotKey is hit.
- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey;

// Called when the local mouse moves
- (void)localMouseMoved:(const SkIPoint&)mousePos;

// Must be called when the LocalInputMonitorImpl is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

// Called to add a host to the list of those to be Shutdown() when the hotkey
// is pressed.
- (void)addHost:(remoting::ChromotingHost*)host;

// Called to remove a host. Returns true if it was the last host being
// monitored, in which case the object should be destroyed.
- (bool)removeHost:(remoting::ChromotingHost*)host;

@end

static CGEventRef LocalMouseMoved(CGEventTapProxy proxy, CGEventType type,
                                  CGEventRef event, void* context) {
  int64_t pid = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID);
  if (pid == 0) {
    CGPoint cgMousePos = CGEventGetLocation(event);
    SkIPoint mousePos = SkIPoint::Make(cgMousePos.x, cgMousePos.y);
    [static_cast<LocalInputMonitorImpl*>(context) localMouseMoved:mousePos];
  }
  return NULL;
}

@implementation LocalInputMonitorImpl

- (id)init {
  if ((self = [super init])) {
    GTMCarbonEventDispatcherHandler* handler =
        [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
    hotKey_ = [handler registerHotKey:kEscKeyCode
                             modifiers:(NSAlternateKeyMask | NSControlKeyMask)
                                target:self
                                action:@selector(hotKeyHit:)
                              userInfo:nil
                           whenPressed:YES];
    if (!hotKey_) {
      LOG(ERROR) << "registerHotKey failed.";
    }
    mouseMachPort_.reset(CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
        1 << kCGEventMouseMoved, LocalMouseMoved, self));
    if (mouseMachPort_) {
      mouseRunLoopSource_ = CFMachPortCreateRunLoopSource(
          NULL, mouseMachPort_, 0);
      CFRunLoopAddSource(
          CFRunLoopGetMain(), mouseRunLoopSource_, kCFRunLoopCommonModes);
    } else {
      LOG(ERROR) << "CGEventTapCreate failed.";
    }
    if (!hotKey_ && !mouseMachPort_) {
      [self release];
      return nil;
    }
  }
  return self;
}

- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey {
  base::AutoLock lock(hostsLock_);
  for (Hosts::const_iterator i = hosts_.begin(); i != hosts_.end(); ++i) {
    (*i)->Shutdown(base::Closure());
  }
}

- (void)localMouseMoved:(const SkIPoint&)mousePos {
  base::AutoLock lock(hostsLock_);
  for (Hosts::const_iterator i = hosts_.begin(); i != hosts_.end(); ++i) {
    (*i)->LocalMouseMoved(mousePos);
  }
}

- (void)invalidate {
  if (hotKey_) {
    GTMCarbonEventDispatcherHandler* handler =
        [GTMCarbonEventDispatcherHandler sharedEventDispatcherHandler];
    [handler unregisterHotKey:hotKey_];
    hotKey_ = NULL;
  }
  if (mouseRunLoopSource_) {
    CFMachPortInvalidate(mouseMachPort_);
    CFRunLoopRemoveSource(
        CFRunLoopGetMain(), mouseRunLoopSource_, kCFRunLoopCommonModes);
    CFRelease(mouseRunLoopSource_);
    mouseMachPort_.reset(0);
    mouseRunLoopSource_ = NULL;
  }
}

- (void)addHost:(remoting::ChromotingHost*)host {
  base::AutoLock lock(hostsLock_);
  hosts_.insert(host);
}

- (bool)removeHost:(remoting::ChromotingHost*)host {
  base::AutoLock lock(hostsLock_);
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

base::LazyInstance<base::Lock,
                   base::LeakyLazyInstanceTraits<base::Lock> >
    monitor_lock = LAZY_INSTANCE_INITIALIZER;
LocalInputMonitorImpl* local_input_monitor = NULL;

}  // namespace

LocalInputMonitorMac::~LocalInputMonitorMac() {
  Stop();
}

void LocalInputMonitorMac::Start(remoting::ChromotingHost* host) {
  base::AutoLock lock(monitor_lock.Get());
  if (!local_input_monitor)
    local_input_monitor = [[LocalInputMonitorImpl alloc] init];
  CHECK(local_input_monitor);
  [local_input_monitor addHost:host];
  host_ = host;
}

void LocalInputMonitorMac::Stop() {
  base::AutoLock lock(monitor_lock.Get());
  if ([local_input_monitor removeHost:host_]) {
    [local_input_monitor invalidate];
    [local_input_monitor release];
    local_input_monitor = nil;
  }
}

remoting::LocalInputMonitor* remoting::LocalInputMonitor::Create() {
  return new LocalInputMonitorMac;
}
