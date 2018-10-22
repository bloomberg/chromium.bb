// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder.h"

#include "base/threading/thread.h"
#include "chromeos/services/assistant/audio_decoder/ipc_data_source.h"
#include "media/base/audio_bus.h"
#include "media/base/data_source.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/blocking_url_protocol.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace chromeos {
namespace assistant {

namespace {

// Preferred bytes per sample when get interleaved data from AudioBus.
constexpr int kBytesPerSample = 2;

void OnError(bool* succeeded) {
  *succeeded = false;
}

}  // namespace

AssistantAudioDecoder::AssistantAudioDecoder(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref,
    mojom::AssistantAudioDecoderClientPtr client,
    mojom::AssistantMediaDataSourcePtr data_source)
    : service_ref_(std::move(service_ref)),
      client_(std::move(client)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      data_source_(std::make_unique<IPCDataSource>(std::move(data_source))),
      media_thread_(std::make_unique<base::Thread>("media_thread")) {
  CHECK(media_thread_->Start());
}

AssistantAudioDecoder::~AssistantAudioDecoder() = default;

void AssistantAudioDecoder::Decode() {
  media_thread_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&AssistantAudioDecoder::DecodeOnMediaThread,
                                base::Unretained(this)));
}

void AssistantAudioDecoder::OpenDecoder(OpenDecoderCallback callback) {
  media_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantAudioDecoder::OpenDecoderOnMediaThread,
                     base::Unretained(this), std::move(callback)));
}

void AssistantAudioDecoder::OpenDecoderOnMediaThread(
    OpenDecoderCallback callback) {
  bool read_ok = true;
  protocol_ = std::make_unique<media::BlockingUrlProtocol>(
      data_source_.get(), base::BindRepeating(&OnError, &read_ok));
  decoder_ = std::make_unique<media::AudioFileReader>(protocol_.get());

  if (!decoder_->Open() || !read_ok) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AssistantAudioDecoder::OnDecoderErrorOnThread,
                       base::Unretained(this), std::move(callback)));
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantAudioDecoder::OnDecoderInitializedOnThread,
                     base::Unretained(this), std::move(callback),
                     decoder_->sample_rate(), decoder_->channels()));
}

void AssistantAudioDecoder::DecodeOnMediaThread() {
  std::vector<std::unique_ptr<media::AudioBus>> decoded_audio_packets;
  // Experimental number of decoded packets before sending to |client_|.
  constexpr int kPacketsToRead = 16;
  decoder_->Read(&decoded_audio_packets, kPacketsToRead);

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AssistantAudioDecoder::OnBufferDecodedOnThread,
                     base::Unretained(this), std::move(decoded_audio_packets)));
}

void AssistantAudioDecoder::OnDecoderInitializedOnThread(
    OpenDecoderCallback callback,
    int sample_rate,
    int channels) {
  std::move(callback).Run(/*success=*/true, kBytesPerSample, sample_rate,
                          channels);
}

void AssistantAudioDecoder::OnBufferDecodedOnThread(
    const std::vector<std::unique_ptr<media::AudioBus>>&
        decoded_audio_packets) {
  std::vector<std::vector<uint8_t>> buffers;
  for (const auto& audio_bus : decoded_audio_packets) {
    const int bytes_to_alloc =
        audio_bus->frames() * kBytesPerSample * audio_bus->channels();
    std::vector<uint8_t> buffer(bytes_to_alloc);
    audio_bus->ToInterleaved(audio_bus->frames(), kBytesPerSample,
                             buffer.data());
    buffers.emplace_back(buffer);
  }
  client_->OnNewBuffers(buffers);
}

void AssistantAudioDecoder::OnDecoderErrorOnThread(
    OpenDecoderCallback callback) {
  std::move(callback).Run(/*success=*/false,
                          /*bytes_per_sample=*/0,
                          /*samples_per_second=*/0,
                          /*channels=*/0);
}

}  // namespace assistant
}  // namespace chromeos
