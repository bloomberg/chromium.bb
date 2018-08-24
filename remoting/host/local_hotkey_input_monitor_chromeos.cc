// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/local_hotkey_input_monitor.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "remoting/host/client_session_control.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"

namespace remoting {

namespace {

class LocalHotkeyInputMonitorChromeos : public LocalHotkeyInputMonitor {
 public:
  LocalHotkeyInputMonitorChromeos(
      scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);
  ~LocalHotkeyInputMonitorChromeos() override;

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
    void HandleKeyPressed(const ui::PlatformEvent& event);

    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

    // Points to the object receiving mouse event notifications and session
    // disconnect requests.  Must be called on the |caller_task_runner_|.
    base::WeakPtr<ClientSessionControl> client_session_control_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  // Task runner on which ui::events are received.
  scoped_refptr<base::SingleThreadTaskRunner> input_task_runner_;
  std::unique_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(LocalHotkeyInputMonitorChromeos);
};

LocalHotkeyInputMonitorChromeos::LocalHotkeyInputMonitorChromeos(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : input_task_runner_(input_task_runner),
      core_(new Core(caller_task_runner, client_session_control)) {
  input_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Core::Start, base::Unretained(core_.get())));
}

LocalHotkeyInputMonitorChromeos::~LocalHotkeyInputMonitorChromeos() {
  input_task_runner_->DeleteSoon(FROM_HERE, core_.release());
}

LocalHotkeyInputMonitorChromeos::Core::Core(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control)
    : caller_task_runner_(caller_task_runner),
      client_session_control_(client_session_control) {
  DCHECK(client_session_control_.get());
}

void LocalHotkeyInputMonitorChromeos::Core::Start() {
  // TODO(erg): Need to handle the mus case where PlatformEventSource is null
  // because we are in mus. This class looks like it can be rewritten with mus
  // EventMatchers. (And if that doesn't work, maybe a PointerObserver.)
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}

LocalHotkeyInputMonitorChromeos::Core::~Core() {
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void LocalHotkeyInputMonitorChromeos::Core::WillProcessEvent(
    const ui::PlatformEvent& event) {
  // No need to handle this callback.
}

void LocalHotkeyInputMonitorChromeos::Core::DidProcessEvent(
    const ui::PlatformEvent& event) {
  ui::EventType type = ui::EventTypeFromNative(event);
  if (type == ui::ET_KEY_PRESSED) {
    HandleKeyPressed(event);
  }
}

void LocalHotkeyInputMonitorChromeos::Core::HandleKeyPressed(
    const ui::PlatformEvent& event) {
  ui::KeyEvent key_event(event);
  DCHECK(key_event.is_char());
  if (key_event.IsControlDown() && key_event.IsAltDown() &&
      key_event.key_code() == ui::VKEY_ESCAPE) {
    caller_task_runner_->PostTask(
        FROM_HERE, base::BindRepeating(&ClientSessionControl::DisconnectSession,
                                       client_session_control_, protocol::OK));
  }
}

}  // namespace

std::unique_ptr<LocalHotkeyInputMonitor> LocalHotkeyInputMonitor::Create(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> input_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    base::WeakPtr<ClientSessionControl> client_session_control) {
  return std::make_unique<LocalHotkeyInputMonitorChromeos>(
      caller_task_runner, input_task_runner, client_session_control);
}

}  // namespace remoting
