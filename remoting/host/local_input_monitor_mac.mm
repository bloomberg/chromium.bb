// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "remoting/host/mouse_move_observer.h"
#include "third_party/skia/include/core/SkPoint.h"
#import "third_party/GTM/AppKit/GTMCarbonEvent.h"

// Esc Key Code is 53.
// http://boredzo.org/blog/wp-content/uploads/2007/05/IMTx-virtual-keycodes.pdf
static const NSUInteger kEscKeyCode = 53;

namespace remoting {

namespace {

class LocalInputMonitorMac : public LocalInputMonitor {
 public:
  LocalInputMonitorMac() : mouse_move_observer_(NULL) {}
  virtual ~LocalInputMonitorMac();
  virtual void Start(MouseMoveObserver* mouse_move_observer,
                     const base::Closure& disconnect_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;

  void OnLocalMouseMoved(const SkIPoint& new_pos);
  void OnDisconnectShortcut();

 private:
  MouseMoveObserver* mouse_move_observer_;
  base::Closure disconnect_callback_;
  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorMac);
};

typedef std::set<remoting::LocalInputMonitorMac*> LocalInputMonitors;

}  // namespace

}  // namespace remoting

@interface LocalInputMonitorManager : NSObject {
 @private
  GTMCarbonHotKey* hotKey_;
  CFRunLoopSourceRef mouseRunLoopSource_;
  base::mac::ScopedCFTypeRef<CFMachPortRef> mouseMachPort_;
  base::Lock lock_;
  remoting::LocalInputMonitors monitors_;
}

// Called when the hotKey is hit.
- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey;

// Called when the local mouse moves
- (void)localMouseMoved:(const SkIPoint&)mousePos;

// Must be called when the LocalInputMonitorManager is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

// Called to add a monitor.
- (void)addMonitor:(remoting::LocalInputMonitorMac*)monitor;

// Called to remove a monitor. Returns true if it was the last
// monitor, in which case the object should be destroyed.
- (bool)removeMonitor:(remoting::LocalInputMonitorMac*)monitor;

@end

static CGEventRef LocalMouseMoved(CGEventTapProxy proxy, CGEventType type,
                                  CGEventRef event, void* context) {
  int64_t pid = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID);
  if (pid == 0) {
    CGPoint cgMousePos = CGEventGetLocation(event);
    SkIPoint mousePos = SkIPoint::Make(cgMousePos.x, cgMousePos.y);
    [static_cast<LocalInputMonitorManager*>(context) localMouseMoved:mousePos];
  }
  return NULL;
}

@implementation LocalInputMonitorManager

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
  base::AutoLock lock(lock_);
  for (remoting::LocalInputMonitors::const_iterator i = monitors_.begin();
       i != monitors_.end(); ++i) {
    (*i)->OnDisconnectShortcut();
  }
}

- (void)localMouseMoved:(const SkIPoint&)mousePos {
  base::AutoLock lock(lock_);
  for (remoting::LocalInputMonitors::const_iterator i = monitors_.begin();
       i != monitors_.end(); ++i) {
    (*i)->OnLocalMouseMoved(mousePos);
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

- (void)addMonitor:(remoting::LocalInputMonitorMac*)monitor {
  base::AutoLock lock(lock_);
  monitors_.insert(monitor);
}

- (bool)removeMonitor:(remoting::LocalInputMonitorMac*)monitor {
  base::AutoLock lock(lock_);
  monitors_.erase(monitor);
  return monitors_.empty();
}

@end

namespace remoting {

namespace {

base::LazyInstance<base::Lock>::Leaky g_monitor_lock =
    LAZY_INSTANCE_INITIALIZER;
LocalInputMonitorManager* g_local_input_monitor_manager = NULL;

LocalInputMonitorMac::~LocalInputMonitorMac() {
  Stop();
}

void LocalInputMonitorMac::Start(MouseMoveObserver* mouse_move_observer,
                                 const base::Closure& disconnect_callback) {
  base::AutoLock lock(g_monitor_lock.Get());
  if (!g_local_input_monitor_manager)
    g_local_input_monitor_manager = [[LocalInputMonitorManager alloc] init];
  CHECK(g_local_input_monitor_manager);
  mouse_move_observer_ = mouse_move_observer;
  disconnect_callback_ = disconnect_callback;
  [g_local_input_monitor_manager addMonitor:this];
}

void LocalInputMonitorMac::Stop() {
  base::AutoLock lock(g_monitor_lock.Get());
  if ([g_local_input_monitor_manager removeMonitor:this]) {
    [g_local_input_monitor_manager invalidate];
    [g_local_input_monitor_manager release];
    g_local_input_monitor_manager = nil;
  }
}

void LocalInputMonitorMac::OnLocalMouseMoved(const SkIPoint& new_pos) {
  mouse_move_observer_->OnLocalMouseMoved(new_pos);
}

void LocalInputMonitorMac::OnDisconnectShortcut() {
  disconnect_callback_.Run();
}

}  // namespace

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create() {
  return scoped_ptr<LocalInputMonitor>(new LocalInputMonitorMac());
}

}  // namespace remoting
