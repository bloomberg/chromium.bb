// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/input_monitor/local_mouse_input_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/chromeos/point_transformer.h"
#include "remoting/host/client_session_control.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"

namespace remoting {

namespace {

class LocalMouseInputMonitorChromeos : public LocalMouseInputMonitor {
 public:
  LocalMouseInputMonitorChromeos(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);
  ~LocalMouseInputMonitorChromeos() override;

 private:
  class Core : ui::PlatformEventObserver {
   public:
    Core(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
         base::WeakPtr<ClientSessionControl> client_session_control);
    ~Core() override;

    void Start();

    // ui::PlatformEventObserver interface.
    void WillProcessEvent(const ui::PlatformEvent& event) override;
    void DidProcessEvent(const ui::PlatformEvent& event) override;

   private:
    void HandleMouseMove(const ui::PlatformEvent& event);

    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Points to the object receiving mouse event notifications and session
    // disconnect requests.  Must be called on the |caller_task_runner_|.
    base::WeakPtr<ClientSessionControl> client_session_control_;

    // Used to rotate the local mouse positions appropriately based on the
    // current display rotation settings.
    std::unique_ptr<PointTransformer> point_transformer_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // Task runner on which ui::events are received.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(LocalMouseInputMonitorChromeos);
};

LocalMouseInputMonitorChromeos::LocalMouseInputMonitorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : input_task_runner_(input_task_runner),
      core_(new Core(caller_task_runner, client_session_control)) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get())));
}

LocalMouseInputMonitorChromeos::~LocalMouseInputMonitorChromeos() {
  input_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

LocalMouseInputMonitorChromeos::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : caller_task_runner_(caller_task_runner),
      client_session_control_(client_session_control) {
  DCHECK(client_session_control_.get());
}

void LocalMouseInputMonitorChromeos::Core::Start() {
  // TODO(erg): Need to handle the mus case where PlatformEventSource is null
  // because we are in mus. This class looks like it can be rewritten with mus
  // EventMatchers. (And if that doesn't work, maybe a PointerObserver.)
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  point_transformer_.reset(new PointTransformer());
}

LocalMouseInputMonitorChromeos::Core::~Core() {
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void LocalMouseInputMonitorChromeos::Core::WillProcessEvent(
    const ui::PlatformEvent& event) {
  // No need to handle this callback.
}

void LocalMouseInputMonitorChromeos::Core::DidProcessEvent(
    const ui::PlatformEvent& event) {
  ui::EventType type = ui::EventTypeFromNative(event);
  if (type == ui::ET_MOUSE_MOVED) {
    HandleMouseMove(event);
  }
}

void LocalMouseInputMonitorChromeos::Core::HandleMouseMove(
    const ui::PlatformEvent& event) {
  auto mouse_position = gfx::PointF(ui::EventLocationFromNative(event));
  mouse_position = point_transformer_->FromScreenCoordinates(mouse_position);

  caller_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ClientSessionControl::OnLocalMouseMoved, client_session_control_,
          webrtc::DesktopVector(mouse_position.x(), mouse_position.y())));
}

}  // namespace

std::unique_ptr<LocalMouseInputMonitor> LocalMouseInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<LocalMouseInputMonitorChromeos>(
      caller_task_runner, input_task_runner, client_session_control);
}

}  // namespace remoting
