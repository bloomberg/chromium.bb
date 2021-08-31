// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/speech_recognition_recognizer_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/services/speech/soda/proto/soda_api.pb.h"
#include "chrome/services/speech/soda/soda_client.h"
#include "components/soda/constants.h"
#include "google_apis/google_api_keys.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/mojo/common/media_type_converters.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace speech {

constexpr char kInvalidAudioDataError[] = "Invalid audio data received.";

// static
const char
    SpeechRecognitionRecognizerImpl::kCaptionBubbleVisibleHistogramName[] =
        "Accessibility.LiveCaption.Duration.CaptionBubbleVisible2";

// static
const char
    SpeechRecognitionRecognizerImpl::kCaptionBubbleHiddenHistogramName[] =
        "Accessibility.LiveCaption.Duration.CaptionBubbleHidden2";

namespace {

// Callback executed by the SODA library on a speech recognition event. The
// callback handle is a void pointer to the SpeechRecognitionRecognizerImpl that
// owns the SODA instance. SpeechRecognitionRecognizerImpl owns the SodaClient
// which owns the instance of SODA and their sequential destruction order
// ensures that this callback will never be called with an invalid callback
// handle to the SpeechRecognitionRecognizerImpl.
void OnSodaResponse(const char* serialized_proto,
                    int length,
                    void* callback_handle) {
  DCHECK(callback_handle);
  soda::chrome::SodaResponse response;
  if (!response.ParseFromArray(serialized_proto, length)) {
    LOG(ERROR) << "Unable to parse result from SODA.";
    return;
  }

  if (response.soda_type() == soda::chrome::SodaResponse::RECOGNITION) {
    soda::chrome::SodaRecognitionResult result = response.recognition_result();
    DCHECK(result.hypothesis_size());
    static_cast<SpeechRecognitionRecognizerImpl*>(callback_handle)
        ->recognition_event_callback()
        .Run(
            std::string(result.hypothesis(0)),
            result.result_type() == soda::chrome::SodaRecognitionResult::FINAL);
  }

  if (response.soda_type() == soda::chrome::SodaResponse::LANGID) {
    // TODO(crbug.com/1175357): Use the langid event to prompt users to switch
    // languages.
    soda::chrome::SodaLangIdEvent event = response.langid_event();

    if (event.confidence_level() >
            static_cast<int>(media::mojom::ConfidenceLevel::kHighlyConfident) ||
        event.confidence_level() <
            static_cast<int>(media::mojom::ConfidenceLevel::kUnknown)) {
      LOG(ERROR) << "Invalid confidence level returned by SODA: "
                 << event.confidence_level();
      return;
    }

    static_cast<SpeechRecognitionRecognizerImpl*>(callback_handle)
        ->language_identification_event_callback()
        .Run(std::string(event.language()),
             static_cast<media::mojom::ConfidenceLevel>(
                 event.confidence_level()));
  }
}

speech::soda::chrome::ExtendedSodaConfigMsg::RecognitionMode
GetSodaSpeechRecognitionMode(
    media::mojom::SpeechRecognitionMode recognition_mode) {
  switch (recognition_mode) {
    case media::mojom::SpeechRecognitionMode::kUnknown:
      return soda::chrome::ExtendedSodaConfigMsg::UNKNOWN;
    case media::mojom::SpeechRecognitionMode::kIme:
      return soda::chrome::ExtendedSodaConfigMsg::IME;
    case media::mojom::SpeechRecognitionMode::kCaption:
      return soda::chrome::ExtendedSodaConfigMsg::CAPTION;
  }
}

}  // namespace

SpeechRecognitionRecognizerImpl::~SpeechRecognitionRecognizerImpl() {
  RecordDuration();
  soda_client_.reset();
}

void SpeechRecognitionRecognizerImpl::Create(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service_impl,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::FilePath& config_path) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SpeechRecognitionRecognizerImpl>(
          std::move(remote), std::move(speech_recognition_service_impl),
          std::move(options), binary_path, config_path),
      std::move(receiver));
}

bool SpeechRecognitionRecognizerImpl::IsMultichannelSupported() {
  return false;
}

void SpeechRecognitionRecognizerImpl::OnRecognitionEvent(
    const std::string& result,
    const bool is_final) {
  if (!client_remote_.is_bound())
    return;
  client_remote_->OnSpeechRecognitionRecognitionEvent(
      media::mojom::SpeechRecognitionResult::New(result, is_final),
      base::BindOnce(&SpeechRecognitionRecognizerImpl::
                         OnSpeechRecognitionRecognitionEventCallback,
                     weak_factory_.GetWeakPtr()));
}

void SpeechRecognitionRecognizerImpl::
    OnSpeechRecognitionRecognitionEventCallback(bool success) {
  is_client_requesting_speech_recognition_ = success;
}

void SpeechRecognitionRecognizerImpl::OnLanguageIdentificationEvent(
    const std::string& language,
    const media::mojom::ConfidenceLevel confidence_level) {
  if (client_remote_.is_bound()) {
    client_remote_->OnLanguageIdentificationEvent(
        media::mojom::LanguageIdentificationEvent::New(language,
                                                       confidence_level));
  }
}

SpeechRecognitionRecognizerImpl::SpeechRecognitionRecognizerImpl(
    mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient> remote,
    base::WeakPtr<SpeechRecognitionServiceImpl> speech_recognition_service_impl,
    media::mojom::SpeechRecognitionOptionsPtr options,
    const base::FilePath& binary_path,
    const base::FilePath& config_path)
    : enable_soda_(base::FeatureList::IsEnabled(media::kUseSodaForLiveCaption)),
      client_remote_(std::move(remote)),
      config_path_(config_path),
      options_(std::move(options)) {
  recognition_event_callback_ = media::BindToCurrentLoop(
      base::BindRepeating(&SpeechRecognitionRecognizerImpl::OnRecognitionEvent,
                          weak_factory_.GetWeakPtr()));
  language_identification_event_callback_ =
      media::BindToCurrentLoop(base::BindRepeating(
          &SpeechRecognitionRecognizerImpl::OnLanguageIdentificationEvent,
          weak_factory_.GetWeakPtr()));

  // Unretained is safe because |this| owns the mojo::Remote.
  client_remote_.set_disconnect_handler(
      base::BindOnce(&SpeechRecognitionRecognizerImpl::OnClientHostDisconnected,
                     weak_factory_.GetWeakPtr()));

  if (enable_soda_) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // On Chrome OS Ash, soda_client_ is not used, so don't try to create it
    // here because it exists at a different location. Instead,
    // CrosSpeechRecognitionRecognizerImpl has its own CrosSodaClient.
    DCHECK(base::PathExists(binary_path));
    soda_client_ = std::make_unique<::soda::SodaClient>(binary_path);
    if (!soda_client_->BinaryLoadedSuccessfully()) {
      OnSpeechRecognitionError();
    }
#endif
  } else {
    cloud_client_ = std::make_unique<CloudSpeechRecognitionClient>(
        recognition_event_callback(),
        std::move(speech_recognition_service_impl));
  }
}

void SpeechRecognitionRecognizerImpl::OnClientHostDisconnected() {
  is_client_requesting_speech_recognition_ = false;
}

void SpeechRecognitionRecognizerImpl::SendAudioToSpeechRecognitionService(
    media::mojom::AudioDataS16Ptr buffer) {
  int channel_count = buffer->channel_count;
  int frame_count = buffer->frame_count;
  int sample_rate = buffer->sample_rate;
  size_t num_samples = 0;
  size_t buffer_size = 0;

  // Update watch time durations.
  base::TimeDelta duration =
      media::AudioTimestampHelper::FramesToTime(frame_count, sample_rate);
  if (is_client_requesting_speech_recognition_) {
    caption_bubble_visible_duration_ += duration;
  } else {
    caption_bubble_hidden_duration_ += duration;
    return;
  }

  // Verify the channel count.
  if (channel_count <= 0 || channel_count > media::limits::kMaxChannels) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify and calculate the number of samples.
  if (sample_rate <= 0 || frame_count <= 0 ||
      !base::CheckMul(frame_count, channel_count).AssignIfValid(&num_samples) ||
      num_samples != buffer->data.size()) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // Verify and calculate the buffer size.
  if (!base::CheckMul(buffer->data.size(), sizeof(buffer->data[0]))
           .AssignIfValid(&buffer_size)) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  // OK, everything is verified, let's send the audio.
  SendAudioToSpeechRecognitionServiceInternal(std::move(buffer));
}

void SpeechRecognitionRecognizerImpl::OnSpeechRecognitionError() {
  if (client_remote_.is_bound()) {
    client_remote_->OnSpeechRecognitionError();
  }
}

void SpeechRecognitionRecognizerImpl::
    SendAudioToSpeechRecognitionServiceInternal(
        media::mojom::AudioDataS16Ptr buffer) {
  channel_count_ = buffer->channel_count;
  sample_rate_ = buffer->sample_rate;
  size_t buffer_size = 0;
  // Verify and calculate the buffer size.
  if (!base::CheckMul(buffer->data.size(), sizeof(buffer->data[0]))
           .AssignIfValid(&buffer_size)) {
    mojo::ReportBadMessage(kInvalidAudioDataError);
    return;
  }

  if (enable_soda_) {
    DCHECK(soda_client_);
    DCHECK(base::PathExists(config_path_));
    if (!soda_client_->IsInitialized() ||
        soda_client_->DidAudioPropertyChange(sample_rate_, channel_count_)) {
      ResetSoda();
    }

    soda_client_->AddAudio(reinterpret_cast<char*>(buffer->data.data()),
                           buffer_size);
  } else {
    DCHECK(cloud_client_);
    if (!cloud_client_->IsInitialized() ||
        cloud_client_->DidAudioPropertyChange(sample_rate_, channel_count_)) {
      // Initialize the stream.
      CloudSpeechConfig config;
      config.sample_rate = sample_rate_;
      config.channel_count = channel_count_;
      config.language_code = "en-US";
      cloud_client_->Initialize(config);
    }

    cloud_client_->AddAudio(base::span<const char>(
        reinterpret_cast<char*>(buffer->data.data()), buffer_size));
  }
}

void SpeechRecognitionRecognizerImpl::OnLanguageChanged(
    const std::string& language) {
  absl::optional<speech::SodaLanguagePackComponentConfig>
      language_component_config = GetLanguageComponentConfig(language);
  if (!language_component_config.has_value())
    return;

  // Only reset SODA if the language changed.
  LanguageCode language_code = language_component_config.value().language_code;
  if (language_code == language_ || language_code == LanguageCode::kNone)
    return;

  language_ = language_component_config.value().language_code;
  base::FilePath config_path = GetLatestSodaLanguagePackDirectory(language);
  if (base::PathExists(config_path)) {
    config_path_ = config_path;
    ResetSoda();
  } else {
    NOTREACHED();
  }
}

void SpeechRecognitionRecognizerImpl::RecordDuration() {
  if (caption_bubble_visible_duration_ > base::TimeDelta()) {
    base::UmaHistogramLongTimes100(kCaptionBubbleVisibleHistogramName,
                                   caption_bubble_visible_duration_);
  }

  if (caption_bubble_hidden_duration_ > base::TimeDelta()) {
    base::UmaHistogramLongTimes100(kCaptionBubbleHiddenHistogramName,
                                   caption_bubble_hidden_duration_);
  }
}

void SpeechRecognitionRecognizerImpl::ResetSoda() {
  // Initialize the SODA instance.
  auto api_key = google_apis::GetSodaAPIKey();
  std::string language_pack_directory = config_path_.AsUTF8Unsafe();

  // Initialize the SODA instance with the serialized config.
  soda::chrome::ExtendedSodaConfigMsg config_msg;
  config_msg.set_channel_count(channel_count_);
  config_msg.set_sample_rate(sample_rate_);
  config_msg.set_api_key(api_key);
  config_msg.set_language_pack_directory(language_pack_directory);
  config_msg.set_simulate_realtime_testonly(false);
  config_msg.set_enable_lang_id(false);
  config_msg.set_recognition_mode(
      GetSodaSpeechRecognitionMode(options_->recognition_mode));
  auto serialized = config_msg.SerializeAsString();

  SerializedSodaConfig config;
  config.soda_config = serialized.c_str();
  config.soda_config_size = serialized.size();
  config.callback = &OnSodaResponse;
  config.callback_handle = this;
  soda_client_->Reset(config, sample_rate_, channel_count_);
}

}  // namespace speech
