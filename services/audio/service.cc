// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/service.h"

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/system/system_monitor.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "components/crash/core/common/crash_key.h"
#include "media/audio/audio_manager.h"
#include "services/audio/debug_recording.h"
#include "services/audio/device_notifier.h"
#include "services/audio/log_factory_manager.h"
#include "services/audio/service_metrics.h"
#include "services/audio/system_info.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

#if defined(OS_MACOSX)
#include "media/audio/mac/audio_device_listener_mac.h"
#endif

namespace audio {

namespace {
// TODO(crbug.com/888478): Remove this after diagnosis.
crash_reporter::CrashKeyString<64> g_service_state_for_crashing(
    "audio-service-state");
}  // namespace

Service::Service(std::unique_ptr<AudioManagerAccessor> audio_manager_accessor,
                 base::TimeDelta quit_timeout,
                 bool enable_remote_client_support,
                 std::unique_ptr<service_manager::BinderRegistry> registry)
    : quit_timeout_(quit_timeout),
      audio_manager_accessor_(std::move(audio_manager_accessor)),
      enable_remote_client_support_(enable_remote_client_support),
      registry_(std::move(registry)) {
  magic_bytes_ = 0x600DC0DEu;
  g_service_state_for_crashing.Set("constructing");
  DCHECK(audio_manager_accessor_);
  if (enable_remote_client_support_) {
    log_factory_manager_ = std::make_unique<LogFactoryManager>();
    audio_manager_accessor_->SetAudioLogFactory(
        log_factory_manager_->GetLogFactory());
  } else {
    // Start device monitoring explicitly if no mojo device notifier will be
    // created. This is required for in-process device notifications.
    InitializeDeviceMonitor();
  }
  g_service_state_for_crashing.Set("constructed");
}

Service::~Service() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_service_state_for_crashing.Set("destructing");
  TRACE_EVENT0("audio", "audio::Service::~Service");

  metrics_.reset();
  g_service_state_for_crashing.Set("destructing - killed metrics");

  // |ref_factory_| may reentrantly call its |quit_closure| when we reset the
  // members below. Destroy it ahead of time to prevent this.
  ref_factory_.reset();
  g_service_state_for_crashing.Set("destructing - killed ref_factory");

  // Stop all streams cleanly before shutting down the audio manager.
  stream_factory_.reset();
  g_service_state_for_crashing.Set("destructing - killed stream_factory");

  // Reset |debug_recording_| to disable debug recording before AudioManager
  // shutdown.
  debug_recording_.reset();
  g_service_state_for_crashing.Set("destructing - killed debug_recording");

  audio_manager_accessor_->Shutdown();
  g_service_state_for_crashing.Set("destructing - did shut down manager");
  magic_bytes_ = 0xDEADBEEFu;
}

void Service::OnStart() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_service_state_for_crashing.Set("starting");
  TRACE_EVENT0("audio", "audio::Service::OnStart")

  // This will pre-create AudioManager if AudioManagerAccessor owns it.
  CHECK(audio_manager_accessor_->GetAudioManager());

  metrics_ =
      std::make_unique<ServiceMetrics>(base::DefaultTickClock::GetInstance());
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      base::BindRepeating(&Service::MaybeRequestQuitDelayed,
                          base::Unretained(this)));
  registry_->AddInterface<mojom::SystemInfo>(base::BindRepeating(
      &Service::BindSystemInfoRequest, base::Unretained(this)));
  registry_->AddInterface<mojom::DebugRecording>(base::BindRepeating(
      &Service::BindDebugRecordingRequest, base::Unretained(this)));
  registry_->AddInterface<mojom::StreamFactory>(base::BindRepeating(
      &Service::BindStreamFactoryRequest, base::Unretained(this)));
  if (enable_remote_client_support_) {
    registry_->AddInterface<mojom::DeviceNotifier>(base::BindRepeating(
        &Service::BindDeviceNotifierRequest, base::Unretained(this)));
    registry_->AddInterface<mojom::LogFactoryManager>(base::BindRepeating(
        &Service::BindLogFactoryManagerRequest, base::Unretained(this)));
  }
  g_service_state_for_crashing.Set("started");
}

void Service::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK(ref_factory_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_service_state_for_crashing.Set("binding " + interface_name);
  TRACE_EVENT1("audio", "audio::Service::OnBindInterface", "interface",
               interface_name);

  if (ref_factory_->HasNoRefs())
    metrics_->HasConnections();

  registry_->BindInterface(interface_name, std::move(interface_pipe));
  DCHECK(!ref_factory_->HasNoRefs());
  quit_timer_.AbandonAndStop();
  g_service_state_for_crashing.Set("bound " + interface_name);
}

bool Service::OnServiceManagerConnectionLost() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  g_service_state_for_crashing.Set("connection lost");
  return true;
}

void Service::SetQuitClosureForTesting(base::RepeatingClosure quit_closure) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  quit_closure_ = std::move(quit_closure);
}

void Service::BindSystemInfoRequest(mojom::SystemInfoRequest request) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);

  if (!system_info_) {
    system_info_ = std::make_unique<SystemInfo>(
        audio_manager_accessor_->GetAudioManager());
  }
  system_info_->Bind(
      std::move(request),
      TracedServiceRef(ref_factory_->CreateRef(), "audio::SystemInfo Binding"));
}

void Service::BindDebugRecordingRequest(mojom::DebugRecordingRequest request) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);
  TracedServiceRef service_ref(ref_factory_->CreateRef(),
                               "audio::DebugRecording Binding");

  // Accept only one bind request at a time. Old request is overwritten.
  // |debug_recording_| must be reset first to disable debug recording, and then
  // create a new DebugRecording instance to enable debug recording.
  debug_recording_.reset();
  debug_recording_ = std::make_unique<DebugRecording>(
      std::move(request), audio_manager_accessor_->GetAudioManager(),
      std::move(service_ref));
}

void Service::BindStreamFactoryRequest(mojom::StreamFactoryRequest request) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);

  if (!stream_factory_)
    stream_factory_.emplace(audio_manager_accessor_->GetAudioManager());
  stream_factory_->Bind(std::move(request),
                        TracedServiceRef(ref_factory_->CreateRef(),
                                         "audio::StreamFactory Binding"));
}

void Service::BindDeviceNotifierRequest(mojom::DeviceNotifierRequest request) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);
  DCHECK(enable_remote_client_support_);

  if (!system_monitor_) {
    CHECK(!base::SystemMonitor::Get());
    system_monitor_ = std::make_unique<base::SystemMonitor>();
  }
  InitializeDeviceMonitor();
  if (!device_notifier_)
    device_notifier_ = std::make_unique<DeviceNotifier>();
  device_notifier_->Bind(std::move(request),
                         TracedServiceRef(ref_factory_->CreateRef(),
                                          "audio::DeviceNotifier Binding"));
}

void Service::BindLogFactoryManagerRequest(
    mojom::LogFactoryManagerRequest request) {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_);
  DCHECK(log_factory_manager_);
  DCHECK(enable_remote_client_support_);
  log_factory_manager_->Bind(
      std::move(request), TracedServiceRef(ref_factory_->CreateRef(),
                                           "audio::LogFactoryManager Binding"));
}

void Service::MaybeRequestQuitDelayed() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  metrics_->HasNoConnections();
  if (quit_timeout_ <= base::TimeDelta())
    return;
  quit_timer_.Start(FROM_HERE, quit_timeout_, this, &Service::MaybeRequestQuit);
}

void Service::MaybeRequestQuit() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ref_factory_ && ref_factory_->HasNoRefs() &&
         quit_timeout_ > base::TimeDelta());
  TRACE_EVENT0("audio", "audio::Service::MaybeRequestQuit");

  context()->CreateQuitClosure().Run();
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void Service::InitializeDeviceMonitor() {
  CHECK_EQ(magic_bytes_, 0x600DC0DEu);
#if defined(OS_MACOSX)
  if (audio_device_listener_mac_)
    return;

  TRACE_EVENT0("audio", "audio::Service::InitializeDeviceMonitor");

  audio_device_listener_mac_ = std::make_unique<media::AudioDeviceListenerMac>(
      base::BindRepeating([] {
        if (base::SystemMonitor::Get()) {
          base::SystemMonitor::Get()->ProcessDevicesChanged(
              base::SystemMonitor::DEVTYPE_AUDIO);
        }
      }),
      true /* monitor_default_input */, true /* monitor_addition_removal */,
      true /* monitor_sources */);
#endif
}

}  // namespace audio
