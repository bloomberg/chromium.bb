// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_mouse_input_monitor.h"

#import <AppKit/AppKit.h>

#include <cstdint>
#include <set>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "remoting/host/client_session_control.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {
namespace {

class LocalMouseInputMonitorMac : public LocalMouseInputMonitor {
 public:
  // Invoked by LocalInputMonitorManager.
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    virtual void OnLocalMouseMoved(const webrtc::DesktopVector& position) = 0;
    virtual void OnDisconnectShortcut() = 0;
  };

  LocalMouseInputMonitorMac(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);
  ~LocalMouseInputMonitorMac() override;

 private:
  // The actual implementation resides in LocalMouseInputMonitorMac::Core class.
  class Core;
  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(LocalMouseInputMonitorMac);
};

}  // namespace
}  // namespace remoting

@interface LocalInputMonitorManager : NSObject {
 @private
  CFRunLoopSourceRef mouseRunLoopSource_;
  base::ScopedCFTypeRef<CFMachPortRef> mouseMachPort_;
  remoting::LocalMouseInputMonitorMac::EventHandler* monitor_;
}

- (id)initWithMonitor:
    (remoting::LocalMouseInputMonitorMac::EventHandler*)monitor;

// Called when the local mouse moves
- (void)localMouseMoved:(const webrtc::DesktopVector&)mousePos;

// Must be called when the LocalInputMonitorManager is no longer to be used.
// Similar to NSTimer in that more than a simple release is required.
- (void)invalidate;

@end

static CGEventRef LocalMouseMoved(CGEventTapProxy proxy, CGEventType type,
                                  CGEventRef event, void* context) {
  int64_t pid = CGEventGetIntegerValueField(event, kCGEventSourceUnixProcessID);
  if (pid == 0) {
    CGPoint cgMousePos = CGEventGetLocation(event);
    webrtc::DesktopVector mousePos(cgMousePos.x, cgMousePos.y);
    [static_cast<LocalInputMonitorManager*>(context) localMouseMoved:mousePos];
  }
  return nullptr;
}

@implementation LocalInputMonitorManager

- (id)initWithMonitor:
    (remoting::LocalMouseInputMonitorMac::EventHandler*)monitor {
  if ((self = [super init])) {
    monitor_ = monitor;

    mouseMachPort_.reset(CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
        1 << kCGEventMouseMoved, LocalMouseMoved, self));
    if (mouseMachPort_) {
      mouseRunLoopSource_ = CFMachPortCreateRunLoopSource(
          nullptr, mouseMachPort_, 0);
      CFRunLoopAddSource(
          CFRunLoopGetMain(), mouseRunLoopSource_, kCFRunLoopCommonModes);
    } else {
      LOG(ERROR) << "CGEventTapCreate failed.";
      [self release];
      return nil;
    }
  }
  return self;
}

- (void)localMouseMoved:(const webrtc::DesktopVector&)mousePos {
  monitor_->OnLocalMouseMoved(mousePos);
}

- (void)invalidate {
  if (mouseRunLoopSource_) {
    CFMachPortInvalidate(mouseMachPort_);
    CFRunLoopRemoveSource(
        CFRunLoopGetMain(), mouseRunLoopSource_, kCFRunLoopCommonModes);
    CFRelease(mouseRunLoopSource_);
    mouseMachPort_.reset(0);
    mouseRunLoopSource_ = nullptr;
  }
}

@end

namespace remoting {
namespace {

class LocalMouseInputMonitorMac::Core : public base::RefCountedThreadSafe<Core>,
                                        public EventHandler {
 public:
  Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
       scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
       base::WeakPtr<ClientSessionControl> client_session_control);

  void Start();
  void Stop();

 private:
  friend class base::RefCountedThreadSafe<Core>;
  ~Core() override;

  void StartOnUiThread();
  void StopOnUiThread();

  // EventHandler interface.
  void OnLocalMouseMoved(const webrtc::DesktopVector& position) override;
  void OnDisconnectShortcut() override;

  // Task runner on which public methods of this class must be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner on which |window_| is created.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  LocalInputMonitorManager* manager_;

  // Invoked in the |caller_task_runner_| thread to report local mouse events
  // and session disconnect requests.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  webrtc::DesktopVector mouse_position_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

LocalMouseInputMonitorMac::LocalMouseInputMonitorMac(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : core_(new Core(caller_task_runner,
                     ui_task_runner,
                     client_session_control)) {
  core_->Start();
}

LocalMouseInputMonitorMac::~LocalMouseInputMonitorMac() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  core_->Stop();
}

LocalMouseInputMonitorMac::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : caller_task_runner_(caller_task_runner),
      ui_task_runner_(ui_task_runner),
      manager_(nil),
      client_session_control_(client_session_control) {
  DCHECK(client_session_control_);
}

void LocalMouseInputMonitorMac::Core::Start() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StartOnUiThread, this));
}

void LocalMouseInputMonitorMac::Core::Stop() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(FROM_HERE,
                            base::BindOnce(&Core::StopOnUiThread, this));
}

LocalMouseInputMonitorMac::Core::~Core() {
  DCHECK(manager_ == nil);
}

void LocalMouseInputMonitorMac::Core::StartOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  manager_ = [[LocalInputMonitorManager alloc] initWithMonitor:this];
}

void LocalMouseInputMonitorMac::Core::StopOnUiThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  [manager_ invalidate];
  [manager_ release];
  manager_ = nil;
}

void LocalMouseInputMonitorMac::Core::OnLocalMouseMoved(
    const webrtc::DesktopVector& position) {
  // In some cases OS may emit bogus mouse-move events even when cursor is not
  // actually moving. To handle this case properly verify that mouse position
  // has changed. See https://crbug.com/360912.
  if (position.equals(mouse_position_)) {
    return;
  }

  mouse_position_ = position;

  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ClientSessionControl::OnLocalMouseMoved,
                                client_session_control_, position));
}

void LocalMouseInputMonitorMac::Core::OnDisconnectShortcut() {
  caller_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ClientSessionControl::DisconnectSession,
                                client_session_control_, protocol::OK));
}

}  // namespace

std::unique_ptr<LocalMouseInputMonitor> LocalMouseInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<LocalMouseInputMonitorMac>(
      caller_task_runner, ui_task_runner, client_session_control);
}

}  // namespace remoting
