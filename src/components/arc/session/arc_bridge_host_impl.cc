// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_bridge_host_impl.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/arc_notifications_host_initializer.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/arc/mojom/appfuse.mojom.h"
#include "components/arc/mojom/audio.mojom.h"
#include "components/arc/mojom/auth.mojom.h"
#include "components/arc/mojom/backup_settings.mojom.h"
#include "components/arc/mojom/bluetooth.mojom.h"
#include "components/arc/mojom/boot_phase_monitor.mojom.h"
#include "components/arc/mojom/camera.mojom.h"
#include "components/arc/mojom/cast_receiver.mojom.h"
#include "components/arc/mojom/cert_store.mojom.h"
#include "components/arc/mojom/clipboard.mojom.h"
#include "components/arc/mojom/crash_collector.mojom.h"
#include "components/arc/mojom/disk_quota.mojom.h"
#include "components/arc/mojom/enterprise_reporting.mojom.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "components/arc/mojom/ime.mojom.h"
#include "components/arc/mojom/input_method_manager.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/mojom/keymaster.mojom.h"
#include "components/arc/mojom/kiosk.mojom.h"
#include "components/arc/mojom/lock_screen.mojom.h"
#include "components/arc/mojom/media_session.mojom.h"
#include "components/arc/mojom/metrics.mojom.h"
#include "components/arc/mojom/midis.mojom.h"
#include "components/arc/mojom/net.mojom.h"
#include "components/arc/mojom/notifications.mojom.h"
#include "components/arc/mojom/obb_mounter.mojom.h"
#include "components/arc/mojom/oemcrypto.mojom.h"
#include "components/arc/mojom/pip.mojom.h"
#include "components/arc/mojom/policy.mojom.h"
#include "components/arc/mojom/power.mojom.h"
#include "components/arc/mojom/print_spooler.mojom.h"
#include "components/arc/mojom/process.mojom.h"
#include "components/arc/mojom/property.mojom.h"
#include "components/arc/mojom/rotation_lock.mojom.h"
#include "components/arc/mojom/screen_capture.mojom.h"
#include "components/arc/mojom/storage_manager.mojom.h"
#include "components/arc/mojom/timer.mojom.h"
#include "components/arc/mojom/tracing.mojom.h"
#include "components/arc/mojom/tts.mojom.h"
#include "components/arc/mojom/usb_host.mojom.h"
#include "components/arc/mojom/video.mojom.h"
#include "components/arc/mojom/voice_interaction_arc_home.mojom.h"
#include "components/arc/mojom/voice_interaction_framework.mojom.h"
#include "components/arc/mojom/volume_mounter.mojom.h"
#include "components/arc/mojom/wake_lock.mojom.h"
#include "components/arc/mojom/wallpaper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/mojo_channel.h"
#include "content/public/browser/system_connector.h"

namespace arc {

ArcBridgeHostImpl::ArcBridgeHostImpl(
    ArcBridgeService* arc_bridge_service,
    mojo::PendingReceiver<mojom::ArcBridgeHost> pending_receiver)
    : arc_bridge_service_(arc_bridge_service),
      receiver_(this, std::move(pending_receiver)) {
  DCHECK(arc_bridge_service_);
  receiver_.set_disconnect_handler(
      base::BindOnce(&ArcBridgeHostImpl::OnClosed, base::Unretained(this)));
}

ArcBridgeHostImpl::~ArcBridgeHostImpl() {
  OnClosed();
}

void ArcBridgeHostImpl::OnAccessibilityHelperInstanceReady(
    mojom::AccessibilityHelperInstancePtr accessibility_helper_ptr) {
  OnInstanceReady(arc_bridge_service_->accessibility_helper(),
                  std::move(accessibility_helper_ptr));
}

void ArcBridgeHostImpl::OnAppInstanceReady(mojom::AppInstancePtr app_ptr) {
  OnInstanceReady(arc_bridge_service_->app(), std::move(app_ptr));
}

void ArcBridgeHostImpl::OnAppPermissionsInstanceReady(
    mojom::AppPermissionsInstancePtr app_permissions_ptr) {
  OnInstanceReady(arc_bridge_service_->app_permissions(),
                  std::move(app_permissions_ptr));
}

void ArcBridgeHostImpl::OnAppfuseInstanceReady(
    mojom::AppfuseInstancePtr appfuse_ptr) {
  OnInstanceReady(arc_bridge_service_->appfuse(), std::move(appfuse_ptr));
}

void ArcBridgeHostImpl::OnAudioInstanceReady(
    mojom::AudioInstancePtr audio_ptr) {
  OnInstanceReady(arc_bridge_service_->audio(), std::move(audio_ptr));
}

void ArcBridgeHostImpl::OnAuthInstanceReady(mojom::AuthInstancePtr auth_ptr) {
  OnInstanceReady(arc_bridge_service_->auth(), std::move(auth_ptr));
}

void ArcBridgeHostImpl::OnBackupSettingsInstanceReady(
    mojom::BackupSettingsInstancePtr backup_settings_ptr) {
  OnInstanceReady(arc_bridge_service_->backup_settings(),
                  std::move(backup_settings_ptr));
}

void ArcBridgeHostImpl::OnBluetoothInstanceReady(
    mojom::BluetoothInstancePtr bluetooth_ptr) {
  OnInstanceReady(arc_bridge_service_->bluetooth(), std::move(bluetooth_ptr));
}

void ArcBridgeHostImpl::OnBootPhaseMonitorInstanceReady(
    mojom::BootPhaseMonitorInstancePtr boot_phase_monitor_ptr) {
  OnInstanceReady(arc_bridge_service_->boot_phase_monitor(),
                  std::move(boot_phase_monitor_ptr));
}

void ArcBridgeHostImpl::OnCameraInstanceReady(
    mojom::CameraInstancePtr camera_ptr) {
  OnInstanceReady(arc_bridge_service_->camera(), std::move(camera_ptr));
}

void ArcBridgeHostImpl::OnCastReceiverInstanceReady(
    mojom::CastReceiverInstancePtr cast_receiver_ptr) {
  OnInstanceReady(arc_bridge_service_->cast_receiver(),
                  std::move(cast_receiver_ptr));
}

void ArcBridgeHostImpl::OnCertStoreInstanceReady(
    mojom::CertStoreInstancePtr instance_ptr) {
  OnInstanceReady(arc_bridge_service_->cert_store(), std::move(instance_ptr));
}

void ArcBridgeHostImpl::OnClipboardInstanceReady(
    mojom::ClipboardInstancePtr clipboard_ptr) {
  OnInstanceReady(arc_bridge_service_->clipboard(), std::move(clipboard_ptr));
}

void ArcBridgeHostImpl::OnCrashCollectorInstanceReady(
    mojom::CrashCollectorInstancePtr crash_collector_ptr) {
  OnInstanceReady(arc_bridge_service_->crash_collector(),
                  std::move(crash_collector_ptr));
}

void ArcBridgeHostImpl::OnDiskQuotaInstanceReady(
    mojom::DiskQuotaInstancePtr disk_quota_ptr) {
  OnInstanceReady(arc_bridge_service_->disk_quota(), std::move(disk_quota_ptr));
}

void ArcBridgeHostImpl::OnEnterpriseReportingInstanceReady(
    mojom::EnterpriseReportingInstancePtr enterprise_reporting_ptr) {
  OnInstanceReady(arc_bridge_service_->enterprise_reporting(),
                  std::move(enterprise_reporting_ptr));
}

void ArcBridgeHostImpl::OnFileSystemInstanceReady(
    mojom::FileSystemInstancePtr file_system_ptr) {
  OnInstanceReady(arc_bridge_service_->file_system(),
                  std::move(file_system_ptr));
}

void ArcBridgeHostImpl::OnImeInstanceReady(mojom::ImeInstancePtr ime_ptr) {
  OnInstanceReady(arc_bridge_service_->ime(), std::move(ime_ptr));
}

void ArcBridgeHostImpl::OnInputMethodManagerInstanceReady(
    mojom::InputMethodManagerInstancePtr input_method_manager_ptr) {
  OnInstanceReady(arc_bridge_service_->input_method_manager(),
                  std::move(input_method_manager_ptr));
}

void ArcBridgeHostImpl::OnIntentHelperInstanceReady(
    mojom::IntentHelperInstancePtr intent_helper_ptr) {
  OnInstanceReady(arc_bridge_service_->intent_helper(),
                  std::move(intent_helper_ptr));
}

void ArcBridgeHostImpl::OnKeymasterInstanceReady(
    mojom::KeymasterInstancePtr keymaster_ptr) {
  OnInstanceReady(arc_bridge_service_->keymaster(), std::move(keymaster_ptr));
}

void ArcBridgeHostImpl::OnKioskInstanceReady(
    mojom::KioskInstancePtr kiosk_ptr) {
  OnInstanceReady(arc_bridge_service_->kiosk(), std::move(kiosk_ptr));
}

void ArcBridgeHostImpl::OnLockScreenInstanceReady(
    mojom::LockScreenInstancePtr lock_screen_ptr) {
  OnInstanceReady(arc_bridge_service_->lock_screen(),
                  std::move(lock_screen_ptr));
}

void ArcBridgeHostImpl::OnMediaSessionInstanceReady(
    mojom::MediaSessionInstancePtr media_session_ptr) {
  OnInstanceReady(arc_bridge_service_->media_session(),
                  std::move(media_session_ptr));
}

void ArcBridgeHostImpl::OnMetricsInstanceReady(
    mojom::MetricsInstancePtr metrics_ptr) {
  OnInstanceReady(arc_bridge_service_->metrics(), std::move(metrics_ptr));
}

void ArcBridgeHostImpl::OnMidisInstanceReady(
    mojom::MidisInstancePtr midis_ptr) {
  OnInstanceReady(arc_bridge_service_->midis(), std::move(midis_ptr));
}

void ArcBridgeHostImpl::OnNetInstanceReady(mojom::NetInstancePtr net_ptr) {
  OnInstanceReady(arc_bridge_service_->net(), std::move(net_ptr));
}

void ArcBridgeHostImpl::OnNotificationsInstanceReady(
    mojom::NotificationsInstancePtr notifications_ptr) {
  // Forward notification instance to ash.
  ash::ArcNotificationsHostInitializer::Get()->SetArcNotificationsInstance(
      notifications_ptr.PassInterface());
}

void ArcBridgeHostImpl::OnObbMounterInstanceReady(
    mojom::ObbMounterInstancePtr obb_mounter_ptr) {
  OnInstanceReady(arc_bridge_service_->obb_mounter(),
                  std::move(obb_mounter_ptr));
}

void ArcBridgeHostImpl::OnOemCryptoInstanceReady(
    mojom::OemCryptoInstancePtr oemcrypto_ptr) {
  OnInstanceReady(arc_bridge_service_->oemcrypto(), std::move(oemcrypto_ptr));
}

void ArcBridgeHostImpl::OnPipInstanceReady(mojom::PipInstancePtr pip_ptr) {
  OnInstanceReady(arc_bridge_service_->pip(), std::move(pip_ptr));
}

void ArcBridgeHostImpl::OnPolicyInstanceReady(
    mojom::PolicyInstancePtr policy_ptr) {
  OnInstanceReady(arc_bridge_service_->policy(), std::move(policy_ptr));
}

void ArcBridgeHostImpl::OnPowerInstanceReady(
    mojom::PowerInstancePtr power_ptr) {
  OnInstanceReady(arc_bridge_service_->power(), std::move(power_ptr));
}

void ArcBridgeHostImpl::OnPrintSpoolerInstanceReady(
    mojom::PrintSpoolerInstancePtr print_spooler_ptr) {
  OnInstanceReady(arc_bridge_service_->print_spooler(),
                  std::move(print_spooler_ptr));
}

void ArcBridgeHostImpl::OnProcessInstanceReady(
    mojom::ProcessInstancePtr process_ptr) {
  OnInstanceReady(arc_bridge_service_->process(), std::move(process_ptr));
}

void ArcBridgeHostImpl::OnPropertyInstanceReady(
    mojom::PropertyInstancePtr property_ptr) {
  OnInstanceReady(arc_bridge_service_->property(), std::move(property_ptr));
}

void ArcBridgeHostImpl::OnRotationLockInstanceReady(
    mojom::RotationLockInstancePtr rotation_lock_ptr) {
  OnInstanceReady(arc_bridge_service_->rotation_lock(),
                  std::move(rotation_lock_ptr));
}

void ArcBridgeHostImpl::OnScreenCaptureInstanceReady(
    mojom::ScreenCaptureInstancePtr screen_capture_ptr) {
  OnInstanceReady(arc_bridge_service_->screen_capture(),
                  std::move(screen_capture_ptr));
}

void ArcBridgeHostImpl::OnSmartCardManagerInstanceReady(
    mojom::SmartCardManagerInstancePtr smart_card_manager_ptr) {
  OnInstanceReady(arc_bridge_service_->smart_card_manager(),
                  std::move(smart_card_manager_ptr));
}

void ArcBridgeHostImpl::OnStorageManagerInstanceReady(
    mojom::StorageManagerInstancePtr storage_manager_ptr) {
  OnInstanceReady(arc_bridge_service_->storage_manager(),
                  std::move(storage_manager_ptr));
}

void ArcBridgeHostImpl::OnTimerInstanceReady(
    mojom::TimerInstancePtr timer_ptr) {
  OnInstanceReady(arc_bridge_service_->timer(), std::move(timer_ptr));
}

void ArcBridgeHostImpl::OnTracingInstanceReady(
    mojom::TracingInstancePtr tracing_ptr) {
  OnInstanceReady(arc_bridge_service_->tracing(), std::move(tracing_ptr));
}

void ArcBridgeHostImpl::OnTtsInstanceReady(mojom::TtsInstancePtr tts_ptr) {
  OnInstanceReady(arc_bridge_service_->tts(), std::move(tts_ptr));
}

void ArcBridgeHostImpl::OnUsbHostInstanceReady(
    mojom::UsbHostInstancePtr usb_host_ptr) {
  OnInstanceReady(arc_bridge_service_->usb_host(), std::move(usb_host_ptr));
}

void ArcBridgeHostImpl::OnVideoInstanceReady(
    mojom::VideoInstancePtr video_ptr) {
  OnInstanceReady(arc_bridge_service_->video(), std::move(video_ptr));
}

void ArcBridgeHostImpl::OnVoiceInteractionArcHomeInstanceReady(
    mojom::VoiceInteractionArcHomeInstancePtr home_ptr) {
  NOTREACHED();
}

void ArcBridgeHostImpl::OnVoiceInteractionFrameworkInstanceReady(
    mojom::VoiceInteractionFrameworkInstancePtr framework_ptr) {
  NOTREACHED();
}

void ArcBridgeHostImpl::OnVolumeMounterInstanceReady(
    mojom::VolumeMounterInstancePtr volume_mounter_ptr) {
  OnInstanceReady(arc_bridge_service_->volume_mounter(),
                  std::move(volume_mounter_ptr));
}

void ArcBridgeHostImpl::OnWakeLockInstanceReady(
    mojom::WakeLockInstancePtr wakelock_ptr) {
  OnInstanceReady(arc_bridge_service_->wake_lock(), std::move(wakelock_ptr));
}

void ArcBridgeHostImpl::OnWallpaperInstanceReady(
    mojom::WallpaperInstancePtr wallpaper_ptr) {
  OnInstanceReady(arc_bridge_service_->wallpaper(), std::move(wallpaper_ptr));
}

void ArcBridgeHostImpl::OnClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "Mojo connection lost";

  arc_bridge_service_->ObserveBeforeArcBridgeClosed();

  // Close all mojo channels.
  mojo_channels_.clear();
  receiver_.reset();

  arc_bridge_service_->ObserveAfterArcBridgeClosed();
}

template <typename InstanceType, typename HostType>
void ArcBridgeHostImpl::OnInstanceReady(
    ConnectionHolder<InstanceType, HostType>* holder,
    mojo::InterfacePtr<InstanceType> ptr) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(receiver_.is_bound());
  DCHECK(ptr.is_bound());

  // Track |channel|'s lifetime via |mojo_channels_| so that it will be
  // closed on ArcBridgeHost/Instance closing or the ArcBridgeHostImpl's
  // destruction.
  auto* channel =
      new MojoChannel<InstanceType, HostType>(holder, ptr.PassInterface());
  mojo_channels_.emplace_back(channel);

  // Since |channel| is managed by |mojo_channels_|, its lifetime is shorter
  // than |this|. Thus, the connection error handler will be invoked only
  // when |this| is alive and base::Unretained is safe here.
  channel->set_disconnect_handler(base::BindOnce(
      &ArcBridgeHostImpl::OnChannelClosed, base::Unretained(this), channel));

  // Call QueryVersion so that the version info is properly stored in the
  // InterfacePtr<T>.
  channel->QueryVersion();
}

void ArcBridgeHostImpl::OnChannelClosed(MojoChannelBase* channel) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojo_channels_.erase(
      std::find_if(mojo_channels_.begin(), mojo_channels_.end(),
                   [channel](std::unique_ptr<MojoChannelBase>& ptr) {
                     return ptr.get() == channel;
                   }));
}

}  // namespace arc
