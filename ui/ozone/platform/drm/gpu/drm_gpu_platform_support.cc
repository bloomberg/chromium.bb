// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_gpu_platform_support.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

void MessageProcessedOnMain(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    const base::Closure& io_thread_task) {
  io_thread_task_runner->PostTask(FROM_HERE, io_thread_task);
}

class DrmGpuPlatformSupportMessageFilter : public IPC::MessageFilter {
 public:
  typedef base::Callback<void(
      const scoped_refptr<base::SingleThreadTaskRunner>&)>
      OnFilterAddedCallback;

  DrmGpuPlatformSupportMessageFilter(
      ScreenManager* screen_manager,
      const OnFilterAddedCallback& on_filter_added_callback,
      IPC::Listener* main_thread_listener)
      : screen_manager_(screen_manager),
        on_filter_added_callback_(on_filter_added_callback),
        main_thread_listener_(main_thread_listener),
        main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

  void OnFilterAdded(IPC::Sender* sender) override {
    io_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(on_filter_added_callback_, io_thread_task_runner_));
  }

  // This code is meant to be very temporary and only as a special case to fix
  // cursor movement jank resulting from slowdowns on the gpu main thread.
  // It handles cursor movement on IO thread when display config is stable
  // and returns it to main thread during transitions.
  bool OnMessageReceived(const IPC::Message& message) override {
    // If this message affects the state needed to set cursor, handle it on
    // the main thread. If a cursor move message arrives but we haven't
    // processed the previous main thread message, keep processing on main
    // until nothing is pending.
    bool cursor_position_message = MessageAffectsCursorPosition(message.type());
    bool cursor_state_message = MessageAffectsCursorState(message.type());

    // Only handle cursor related messages here.
    if (!cursor_position_message && !cursor_state_message)
      return false;

    bool cursor_was_animating = cursor_animating_;
    UpdateAnimationState(message);
    if (cursor_state_message || pending_main_thread_operations_ ||
        cursor_animating_ || cursor_was_animating) {
      pending_main_thread_operations_++;

      base::Closure main_thread_message_handler =
          base::Bind(base::IgnoreResult(&IPC::Listener::OnMessageReceived),
                     base::Unretained(main_thread_listener_), message);
      main_thread_task_runner_->PostTask(FROM_HERE,
                                         main_thread_message_handler);

      // This is an echo from the main thread to decrement pending ops.
      // When the main thread is done with the task, it posts back to IO to
      // signal completion.
      base::Closure io_thread_task = base::Bind(
          &DrmGpuPlatformSupportMessageFilter::DecrementPendingOperationsOnIO,
          this);

      base::Closure message_processed_callback = base::Bind(
          &MessageProcessedOnMain, io_thread_task_runner_, io_thread_task);
      main_thread_task_runner_->PostTask(FROM_HERE, message_processed_callback);

      return true;
    }

    // Otherwise, we are in a steady state and it's safe to move cursor on IO.
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupportMessageFilter, message)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)
    IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
    IPC_MESSAGE_UNHANDLED(handled = false);
    IPC_END_MESSAGE_MAP()

    return handled;
  }

 protected:
  ~DrmGpuPlatformSupportMessageFilter() override {}

  void OnCursorMove(gfx::AcceleratedWidget widget, const gfx::Point& location) {
    screen_manager_->GetWindow(widget)->MoveCursor(location);
  }

  void OnCursorSet(gfx::AcceleratedWidget widget,
                   const std::vector<SkBitmap>& bitmaps,
                   const gfx::Point& location,
                   int frame_delay_ms) {
    screen_manager_->GetWindow(widget)
        ->SetCursorWithoutAnimations(bitmaps, location);
  }

  void DecrementPendingOperationsOnIO() { pending_main_thread_operations_--; }

  bool MessageAffectsCursorState(uint32 message_type) {
    switch (message_type) {
      case OzoneGpuMsg_CreateWindow::ID:
      case OzoneGpuMsg_DestroyWindow::ID:
      case OzoneGpuMsg_WindowBoundsChanged::ID:
      case OzoneGpuMsg_ConfigureNativeDisplay::ID:
      case OzoneGpuMsg_DisableNativeDisplay::ID:
        return true;
      default:
        return false;
    }
  }

  bool MessageAffectsCursorPosition(uint32 message_type) {
    switch (message_type) {
      case OzoneGpuMsg_CursorMove::ID:
      case OzoneGpuMsg_CursorSet::ID:
        return true;
      default:
        return false;
    }
  }

  void UpdateAnimationState(const IPC::Message& message) {
    if (message.type() != OzoneGpuMsg_CursorSet::ID)
      return;

    OzoneGpuMsg_CursorSet::Param param;
    if (!OzoneGpuMsg_CursorSet::Read(&message, &param))
      return;

    int frame_delay_ms = base::get<3>(param);
    cursor_animating_ = frame_delay_ms != 0;
  }

  ScreenManager* screen_manager_;
  OnFilterAddedCallback on_filter_added_callback_;
  IPC::Listener* main_thread_listener_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;
  int32 pending_main_thread_operations_ = 0;
  bool cursor_animating_ = false;
};
}

DrmGpuPlatformSupport::DrmGpuPlatformSupport(
    DrmDeviceManager* drm_device_manager,
    ScreenManager* screen_manager,
    ScanoutBufferGenerator* buffer_generator,
    scoped_ptr<DrmGpuDisplayManager> display_manager)
    : drm_device_manager_(drm_device_manager),
      screen_manager_(screen_manager),
      buffer_generator_(buffer_generator),
      display_manager_(display_manager.Pass()) {
  filter_ = new DrmGpuPlatformSupportMessageFilter(
      screen_manager, base::Bind(&DrmGpuPlatformSupport::SetIOTaskRunner,
                                 base::Unretained(this)),
      this);
}

DrmGpuPlatformSupport::~DrmGpuPlatformSupport() {
}

void DrmGpuPlatformSupport::OnChannelEstablished(IPC::Sender* sender) {
  sender_ = sender;
}

bool DrmGpuPlatformSupport::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(DrmGpuPlatformSupport, message)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CreateWindow, OnCreateWindow)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DestroyWindow, OnDestroyWindow)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_WindowBoundsChanged, OnWindowBoundsChanged)

  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorSet, OnCursorSet)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CursorMove, OnCursorMove)

  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RefreshNativeDisplays,
                      OnRefreshNativeDisplays)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_ConfigureNativeDisplay,
                      OnConfigureNativeDisplay)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_DisableNativeDisplay, OnDisableNativeDisplay)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_TakeDisplayControl, OnTakeDisplayControl)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RelinquishDisplayControl,
                      OnRelinquishDisplayControl)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_AddGraphicsDevice, OnAddGraphicsDevice)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_RemoveGraphicsDevice, OnRemoveGraphicsDevice)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_GetHDCPState, OnGetHDCPState)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_SetHDCPState, OnSetHDCPState)
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_SetGammaRamp, OnSetGammaRamp);
  IPC_MESSAGE_HANDLER(OzoneGpuMsg_CheckOverlayCapabilities,
                      OnCheckOverlayCapabilities)
  IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DrmGpuPlatformSupport::OnCreateWindow(gfx::AcceleratedWidget widget) {
  scoped_ptr<DrmWindow> window(
      new DrmWindow(widget, drm_device_manager_, screen_manager_));
  window->Initialize();
  screen_manager_->AddWindow(widget, window.Pass());
}

void DrmGpuPlatformSupport::OnDestroyWindow(gfx::AcceleratedWidget widget) {
  scoped_ptr<DrmWindow> window = screen_manager_->RemoveWindow(widget);
  window->Shutdown();
}

void DrmGpuPlatformSupport::OnWindowBoundsChanged(gfx::AcceleratedWidget widget,
                                                  const gfx::Rect& bounds) {
  screen_manager_->GetWindow(widget)->OnBoundsChanged(bounds);
}

void DrmGpuPlatformSupport::OnCursorSet(gfx::AcceleratedWidget widget,
                                        const std::vector<SkBitmap>& bitmaps,
                                        const gfx::Point& location,
                                        int frame_delay_ms) {
  screen_manager_->GetWindow(widget)
      ->SetCursor(bitmaps, location, frame_delay_ms);
}

void DrmGpuPlatformSupport::OnCursorMove(gfx::AcceleratedWidget widget,
                                         const gfx::Point& location) {
  screen_manager_->GetWindow(widget)->MoveCursor(location);
}

void DrmGpuPlatformSupport::OnCheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays) {
  sender_->Send(new OzoneHostMsg_OverlayCapabilitiesReceived(
      widget, screen_manager_->GetWindow(widget)
                  ->TestPageFlip(overlays, buffer_generator_)));
}

void DrmGpuPlatformSupport::OnRefreshNativeDisplays() {
  sender_->Send(
      new OzoneHostMsg_UpdateNativeDisplays(display_manager_->GetDisplays()));
}

void DrmGpuPlatformSupport::OnConfigureNativeDisplay(
    int64_t id,
    const DisplayMode_Params& mode_param,
    const gfx::Point& origin) {
  sender_->Send(new OzoneHostMsg_DisplayConfigured(
      id, display_manager_->ConfigureDisplay(id, mode_param, origin)));
}

void DrmGpuPlatformSupport::OnDisableNativeDisplay(int64_t id) {
  sender_->Send(new OzoneHostMsg_DisplayConfigured(
      id, display_manager_->DisableDisplay(id)));
}

void DrmGpuPlatformSupport::OnTakeDisplayControl() {
  sender_->Send(new OzoneHostMsg_DisplayControlTaken(
      display_manager_->TakeDisplayControl()));
}

void DrmGpuPlatformSupport::OnRelinquishDisplayControl() {
  display_manager_->RelinquishDisplayControl();
  sender_->Send(new OzoneHostMsg_DisplayControlRelinquished(true));
}

void DrmGpuPlatformSupport::OnAddGraphicsDevice(
    const base::FilePath& path,
    const base::FileDescriptor& fd) {
  drm_device_manager_->AddDrmDevice(path, fd);
}

void DrmGpuPlatformSupport::OnRemoveGraphicsDevice(const base::FilePath& path) {
  drm_device_manager_->RemoveDrmDevice(path);
}

void DrmGpuPlatformSupport::OnSetGammaRamp(
    int64_t id,
    const std::vector<GammaRampRGBEntry>& lut) {
  display_manager_->SetGammaRamp(id, lut);
}

void DrmGpuPlatformSupport::OnGetHDCPState(int64_t display_id) {
  HDCPState state = HDCP_STATE_UNDESIRED;
  bool success = display_manager_->GetHDCPState(display_id, &state);
  sender_->Send(new OzoneHostMsg_HDCPStateReceived(display_id, success, state));
}

void DrmGpuPlatformSupport::OnSetHDCPState(int64_t display_id,
                                           HDCPState state) {
  sender_->Send(new OzoneHostMsg_HDCPStateUpdated(
      display_id, display_manager_->SetHDCPState(display_id, state)));
}

void DrmGpuPlatformSupport::SetIOTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner) {
  drm_device_manager_->InitializeIOTaskRunner(io_task_runner);
}

IPC::MessageFilter* DrmGpuPlatformSupport::GetMessageFilter() {
  return filter_.get();
}

}  // namespace ui
