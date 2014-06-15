// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_linux.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

namespace {

base::LazyInstance<scoped_refptr<AudioPipeReader> >::Leaky
    g_pulseaudio_pipe_sink_reader = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// TODO(wez): Remove this and have the DesktopEnvironmentFactory own the
// AudioPipeReader rather than having it process-global.
// See crbug.com/161373 and crbug.com/104544.
void AudioCapturerLinux::InitializePipeReader(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::FilePath& pipe_name) {
  scoped_refptr<AudioPipeReader> pipe_reader;
  if (!pipe_name.empty())
    pipe_reader = AudioPipeReader::Create(task_runner, pipe_name);
  g_pulseaudio_pipe_sink_reader.Get() = pipe_reader;
}

AudioCapturerLinux::AudioCapturerLinux(
    scoped_refptr<AudioPipeReader> pipe_reader)
    : pipe_reader_(pipe_reader),
      silence_detector_(0) {
}

AudioCapturerLinux::~AudioCapturerLinux() {
}

bool AudioCapturerLinux::Start(const PacketCapturedCallback& callback) {
  callback_ = callback;
  silence_detector_.Reset(AudioPipeReader::kSamplingRate,
                          AudioPipeReader::kChannels);
  pipe_reader_->AddObserver(this);
  return true;
}

void AudioCapturerLinux::Stop() {
  pipe_reader_->RemoveObserver(this);
  callback_.Reset();
}

bool AudioCapturerLinux::IsStarted() {
  return !callback_.is_null();
}

void AudioCapturerLinux::OnDataRead(
    scoped_refptr<base::RefCountedString> data) {
  DCHECK(!callback_.is_null());

  if (silence_detector_.IsSilence(
          reinterpret_cast<const int16*>(data->data().data()),
          data->data().size() / sizeof(int16))) {
    return;
  }

  scoped_ptr<AudioPacket> packet(new AudioPacket());
  packet->add_data(data->data());
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(AudioPipeReader::kSamplingRate);
  packet->set_bytes_per_sample(AudioPipeReader::kBytesPerSample);
  packet->set_channels(AudioPipeReader::kChannels);
  callback_.Run(packet.Pass());
}

bool AudioCapturer::IsSupported() {
  return g_pulseaudio_pipe_sink_reader.Get().get() != NULL;
}

scoped_ptr<AudioCapturer> AudioCapturer::Create() {
  scoped_refptr<AudioPipeReader> reader =
      g_pulseaudio_pipe_sink_reader.Get();
  if (!reader.get())
    return scoped_ptr<AudioCapturer>();
  return scoped_ptr<AudioCapturer>(new AudioCapturerLinux(reader));
}

}  // namespace remoting
