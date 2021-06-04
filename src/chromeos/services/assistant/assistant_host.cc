// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/assistant_host.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "chromeos/services/assistant/libassistant_service_host.h"

namespace chromeos {
namespace assistant {

AssistantHost::AssistantHost() {
  background_thread_.Start();
}

AssistantHost::~AssistantHost() {
  StopLibassistantService();
}

void AssistantHost::Initialize(LibassistantServiceHost* host) {
  DCHECK(host);
  libassistant_service_host_ = host;
  LaunchLibassistantService();

  BindControllers();
}

void AssistantHost::LaunchLibassistantService() {
  // A Mojom service runs on the thread where its receiver was bound.
  // So to make |libassistant_service_| run on the background thread, we must
  // create it on the background thread, as it binds its receiver in its
  // constructor.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AssistantHost::LaunchLibassistantServiceOnBackgroundThread,
          // This is safe because we own the background thread,
          // so when we're deleted the background thread is stopped.
          base::Unretained(this),
          // |libassistant_service_| runs on the current thread, so must
          // be bound here and not on the background thread.
          libassistant_service_.BindNewPipeAndPassReceiver()));
}

void AssistantHost::LaunchLibassistantServiceOnBackgroundThread(
    mojo::PendingReceiver<chromeos::libassistant::mojom::LibassistantService>
        client) {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  DCHECK(libassistant_service_host_);
  libassistant_service_host_->Launch(std::move(client));
}

void AssistantHost::StopLibassistantService() {
  libassistant_service_.reset();

  // |libassistant_service_| is launched on the background thread, so we have to
  // stop it there as well.
  background_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantHost::StopLibassistantServiceOnBackgroundThread,
                     base::Unretained(this)));
}

void AssistantHost::StopLibassistantServiceOnBackgroundThread() {
  DCHECK(background_task_runner()->BelongsToCurrentThread());
  libassistant_service_host_->Stop();
}

void AssistantHost::BindControllers() {
  mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
      pending_audio_input_controller_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::AudioOutputDelegate>
      pending_audio_output_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::DeviceSettingsDelegate>
      pending_device_settings_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::MediaDelegate>
      pending_media_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::NotificationDelegate>
      pending_notification_delegate_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::PlatformDelegate>
      pending_platform_delegate_remote;
  mojo::PendingRemote<
      chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
      pending_speaker_id_enrollment_controller_remote;
  mojo::PendingRemote<chromeos::libassistant::mojom::TimerDelegate>
      pending_timer_delegate_remote;

  media_delegate_ =
      pending_media_delegate_remote.InitWithNewPipeAndPassReceiver();
  notification_delegate_ =
      pending_notification_delegate_remote.InitWithNewPipeAndPassReceiver();
  platform_delegate_ =
      pending_platform_delegate_remote.InitWithNewPipeAndPassReceiver();
  timer_delegate_ =
      pending_timer_delegate_remote.InitWithNewPipeAndPassReceiver();

  pending_audio_output_delegate_receiver_ =
      pending_audio_output_delegate_remote.InitWithNewPipeAndPassReceiver();
  pending_device_settings_delegate_receiver_ =
      pending_device_settings_delegate_remote.InitWithNewPipeAndPassReceiver();
  libassistant_service_->Bind(
      pending_audio_input_controller_remote.InitWithNewPipeAndPassReceiver(),
      conversation_controller_.BindNewPipeAndPassReceiver(),
      display_controller_.BindNewPipeAndPassReceiver(),
      media_controller_.BindNewPipeAndPassReceiver(),
      service_controller_.BindNewPipeAndPassReceiver(),
      settings_controller_.BindNewPipeAndPassReceiver(),
      pending_speaker_id_enrollment_controller_remote
          .InitWithNewPipeAndPassReceiver(),
      timer_controller_.BindNewPipeAndPassReceiver(),
      std::move(pending_audio_output_delegate_remote),
      std::move(pending_device_settings_delegate_remote),
      std::move(pending_media_delegate_remote),
      std::move(pending_notification_delegate_remote),
      std::move(pending_platform_delegate_remote),
      std::move(pending_timer_delegate_remote));

  audio_input_controller_ = std::move(pending_audio_input_controller_remote);
  speaker_id_enrollment_controller_ =
      std::move(pending_speaker_id_enrollment_controller_remote);
}

scoped_refptr<base::SingleThreadTaskRunner>
AssistantHost::background_task_runner() {
  return background_thread_.task_runner();
}

mojo::PendingRemote<chromeos::libassistant::mojom::AudioInputController>
AssistantHost::ExtractAudioInputController() {
  DCHECK(audio_input_controller_.is_valid());
  return std::move(audio_input_controller_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::AudioOutputDelegate>
AssistantHost::ExtractAudioOutputDelegate() {
  DCHECK(pending_audio_output_delegate_receiver_.is_valid());
  return std::move(pending_audio_output_delegate_receiver_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::DeviceSettingsDelegate>
AssistantHost::ExtractDeviceSettingsDelegate() {
  DCHECK(pending_device_settings_delegate_receiver_.is_valid());
  return std::move(pending_device_settings_delegate_receiver_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
AssistantHost::ExtractMediaDelegate() {
  DCHECK(media_delegate_.is_valid());
  return std::move(media_delegate_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::NotificationDelegate>
AssistantHost::ExtractNotificationDelegate() {
  DCHECK(notification_delegate_.is_valid());
  return std::move(notification_delegate_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::PlatformDelegate>
AssistantHost::ExtractPlatformDelegate() {
  DCHECK(platform_delegate_.is_valid());
  return std::move(platform_delegate_);
}

mojo::PendingRemote<
    chromeos::libassistant::mojom::SpeakerIdEnrollmentController>
AssistantHost::ExtractSpeakerIdEnrollmentController() {
  DCHECK(speaker_id_enrollment_controller_.is_valid());
  return std::move(speaker_id_enrollment_controller_);
}

mojo::PendingReceiver<chromeos::libassistant::mojom::TimerDelegate>
AssistantHost::ExtractTimerDelegate() {
  DCHECK(timer_delegate_.is_valid());
  return std::move(timer_delegate_);
}

chromeos::libassistant::mojom::ConversationController&
AssistantHost::conversation_controller() {
  DCHECK(conversation_controller_.is_bound());
  return *conversation_controller_;
}

chromeos::libassistant::mojom::DisplayController&
AssistantHost::display_controller() {
  DCHECK(display_controller_.is_bound());
  return *display_controller_.get();
}

chromeos::libassistant::mojom::ServiceController&
AssistantHost::service_controller() {
  DCHECK(service_controller_.is_bound());
  return *service_controller_.get();
}

chromeos::libassistant::mojom::MediaController&
AssistantHost::media_controller() {
  DCHECK(media_controller_.is_bound());
  return *media_controller_.get();
}

chromeos::libassistant::mojom::SettingsController&
AssistantHost::settings_controller() {
  DCHECK(settings_controller_.is_bound());
  return *settings_controller_;
}

chromeos::libassistant::mojom::TimerController&
AssistantHost::timer_controller() {
  DCHECK(timer_controller_.is_bound());
  return *timer_controller_.get();
}

void AssistantHost::AddSpeechRecognitionObserver(
    mojo::PendingRemote<
        chromeos::libassistant::mojom::SpeechRecognitionObserver> observer) {
  libassistant_service_->AddSpeechRecognitionObserver(std::move(observer));
}

void AssistantHost::AddAuthenticationStateObserver(
    mojo::PendingRemote<
        chromeos::libassistant::mojom::AuthenticationStateObserver> observer) {
  libassistant_service_->AddAuthenticationStateObserver(std::move(observer));
}

}  // namespace assistant
}  // namespace chromeos
