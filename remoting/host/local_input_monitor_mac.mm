// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_input_monitor.h"

#import <AppKit/AppKit.h>
#include <set>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
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
  // Invoked by LocalInputMonitorManager.
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    virtual void OnLocalMouseMoved(const SkIPoint& position) = 0;
    virtual void OnDisconnectShortcut() = 0;
  };

  LocalInputMonitorMac(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  virtual ~LocalInputMonitorMac();

  virtual void Start(MouseMoveObserver* mouse_move_observer,
                     const base::Closure& disconnect_callback) OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  // The actual implementation resides in LocalInputMonitorMac::Core class.
  class Core;
  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(LocalInputMonitorMac);
};

}  // namespace
}  // namespace remoting

@interface LocalInputMonitorManager : NSObject {
 @private
  GTMCarbonHotKey* hotKey_;
  CFRunLoopSourceRef mouseRunLoopSource_;
  base::mac::ScopedCFTypeRef<CFMachPortRef> mouseMachPort_;
  remoting::LocalInputMonitorMac::EventHandler* monitor_;
}

- (id)initWithMonitor:(remoting::LocalInputMonitorMac::EventHandler*)monitor;

// Called when the hotKey is hit.
- (void)hotKeyHit:(GTMCarbonHotKey*)hotKey;

// Called when the local mouse moves
- (void)localMouseMoved:(const SkIPoint&)mousePos;

// Must be called when the LocalInputMonitorManager is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

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

- (id)initWithMonitor:(remoting::LocalInputMonitorMac::EventHandler*)monitor {
  if ((self = [super init])) {
    monitor_ = monitor;

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
  monitor_->OnDisconnectShortcut();
}

- (void)localMouseMoved:(const SkIPoint&)mousePos {
  monitor_->OnLocalMouseMoved(mousePos);
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

@end

namespace remoting {
namespace {

class LocalInputMonitorMac::Core
    : public base::RefCountedThreadSafe<Core>,
      public EventHandler {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  void Start(MouseMoveObserver* mouse_move_observer,
             const base::Closure& disconnect_callback);
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  virtual ~Core();

  void StartOnUiThread();
  void StopOnUiThread();

  // EventHandler interface.
  virtual void OnLocalMouseMoved(const SkIPoint& position) OVERRIDE;
  virtual void OnDisconnectShortcut() OVERRIDE;

  // Task runner on which public methods of this class must be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |window_| is created.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  LocalInputMonitorManager* manager_;

  // Invoked in the |caller_task_runner_| thread to report local mouse events.
  MouseMoveObserver* mouse_move_observer_;

  // Posted to the |caller_task_runner_| thread every time the disconnect key
  // combination is pressed.
  base::Closure disconnect_callback_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

LocalInputMonitorMac::LocalInputMonitorMac(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : core_(new Core(caller_task_runner, ui_task_runner)) {
}

LocalInputMonitorMac::~LocalInputMonitorMac() {
}

void LocalInputMonitorMac::Start(MouseMoveObserver* mouse_move_observer,
                                 const base::Closure& disconnect_callback) {
  core_->Start(mouse_move_observer, disconnect_callback);
}

void LocalInputMonitorMac::Stop() {
  core_->Stop();
}

LocalInputMonitorMac::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      manager_(nil),
      mouse_move_observer_(NULL) {
}

void LocalInputMonitorMac::Core::Start(
    MouseMoveObserver* mouse_move_observer,
    const base::Closure& disconnect_callback) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  DCHECK(disconnect_callback_.is_null());
  DCHECK(!disconnect_callback.is_null());
  DCHECK(!mouse_move_observer_);
  DCHECK(mouse_move_observer);

  disconnect_callback_ = disconnect_callback;
  mouse_move_observer_ = mouse_move_observer;
  ui_task_runner_->PostTask(FROM_HERE,
                            base::Bind(&Core::StartOnUiThread, this));
}

void LocalInputMonitorMac::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  mouse_move_observer_ = NULL;
  disconnect_callback_.Reset();
  ui_task_runner_->PostTask(FROM_HERE, base::Bind(&Core::StopOnUiThread, this));
}

LocalInputMonitorMac::Core::~Core() {
  DCHECK(manager_ == nil);
  DCHECK(disconnect_callback_.is_null());
  DCHECK(!mouse_move_observer_);
}

void LocalInputMonitorMac::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  manager_ = [[LocalInputMonitorManager alloc] initWithMonitor:this];
}

void LocalInputMonitorMac::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  [manager_ invalidate];
  [manager_ release];
  manager_ = nil;
}

void LocalInputMonitorMac::Core::OnLocalMouseMoved(const SkIPoint& position) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::OnLocalMouseMoved, this, position));
    return;
  }

  if (mouse_move_observer_)
    mouse_move_observer_->OnLocalMouseMoved(position);
}

void LocalInputMonitorMac::Core::OnDisconnectShortcut() {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::Bind(&Core::OnDisconnectShortcut, this));
    return;
  }

  if (!disconnect_callback_.is_null())
    disconnect_callback_.Run();
}

}  // namespace

scoped_ptr<LocalInputMonitor> LocalInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  return scoped_ptr<LocalInputMonitor>(
      new LocalInputMonitorMac(caller_task_runner, ui_task_runner));
}

}  // namespace remoting
