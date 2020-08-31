// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/message_parsing_utils.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

#include "base/big_endian.h"
#include "base/logging.h"
#include "chromecast/media/audio/capture_service/packet_header.h"
#include "media/base/limits.h"

namespace chromecast {
namespace media {
namespace capture_service {
namespace {

// Size in bytes of the total/message header.
constexpr size_t kTotalHeaderBytes = 16;
constexpr size_t kMessageHeaderBytes = kTotalHeaderBytes - sizeof(uint16_t);

static_assert(sizeof(PacketHeader) == kTotalHeaderBytes,
              "Invalid packet header size.");
static_assert(offsetof(struct PacketHeader, message_type) == sizeof(uint16_t),
              "Invalid message header offset.");

// Check if audio data is properly aligned and has valid frame size. Return the
// number of frames if they are all good, otherwise return 0 to indicate
// failure.
template <typename T>
int CheckAudioData(int channels, const char* data, size_t data_size) {
  if (reinterpret_cast<const uintptr_t>(data) % sizeof(T) != 0u) {
    LOG(ERROR) << "Misaligned audio data";
    return 0;
  }

  const int frame_size = channels * sizeof(T);
  if (frame_size == 0) {
    LOG(ERROR) << "Frame size is 0.";
    return 0;
  }
  const int frames = data_size / frame_size;
  if (data_size % frame_size != 0) {
    LOG(ERROR) << "Audio data size (" << data_size
               << ") is not an integer number of frames (" << frame_size
               << ").";
    return 0;
  }
  return frames;
}

template <typename Traits>
bool ConvertInterleavedData(int channels,
                            const char* data,
                            size_t data_size,
                            ::media::AudioBus* audio) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  audio->FromInterleaved<Traits>(
      reinterpret_cast<const typename Traits::ValueType*>(data), frames);
  return true;
}

template <typename Traits>
bool ConvertPlanarData(int channels,
                       const char* data,
                       size_t data_size,
                       ::media::AudioBus* audio) {
  const int frames =
      CheckAudioData<typename Traits::ValueType>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  const typename Traits::ValueType* base_data =
      reinterpret_cast<const typename Traits::ValueType*>(data);
  for (int c = 0; c < channels; ++c) {
    const typename Traits::ValueType* source = base_data + c * frames;
    float* dest = audio->channel(c);
    for (int f = 0; f < frames; ++f) {
      dest[f] = Traits::ToFloat(source[f]);
    }
  }
  return true;
}

bool ConvertPlanarFloat(int channels,
                        const char* data,
                        size_t data_size,
                        ::media::AudioBus* audio) {
  const int frames = CheckAudioData<float>(channels, data, data_size);
  if (frames <= 0) {
    return false;
  }

  DCHECK_EQ(frames, audio->frames());
  const float* base_data = reinterpret_cast<const float*>(data);
  for (int c = 0; c < channels; ++c) {
    const float* source = base_data + c * frames;
    std::copy(source, source + frames, audio->channel(c));
  }
  return true;
}

bool ConvertData(int channels,
                 SampleFormat format,
                 const char* data,
                 size_t data_size,
                 ::media::AudioBus* audio) {
  switch (format) {
    case capture_service::SampleFormat::INTERLEAVED_INT16:
      return ConvertInterleavedData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::INTERLEAVED_INT32:
      return ConvertInterleavedData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::INTERLEAVED_FLOAT:
      return ConvertInterleavedData<::media::Float32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_INT16:
      return ConvertPlanarData<::media::SignedInt16SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_INT32:
      return ConvertPlanarData<::media::SignedInt32SampleTypeTraits>(
          channels, data, data_size, audio);
    case capture_service::SampleFormat::PLANAR_FLOAT:
      return ConvertPlanarFloat(channels, data, data_size, audio);
  }
  LOG(ERROR) << "Unknown sample format " << static_cast<int>(format);
  return false;
}

bool HasPacketHeader(MessageType type) {
  return type == MessageType::kAudio || type == MessageType::kAck;
}

}  // namespace

char* PopulateHeader(char* data, size_t size, const PacketInfo& packet_info) {
  DCHECK(HasPacketHeader(packet_info.message_type));
  const StreamInfo& stream_info = packet_info.stream_info;
  PacketHeader header;
  header.message_type = static_cast<uint8_t>(packet_info.message_type);
  header.stream_type = static_cast<uint8_t>(stream_info.stream_type);
  header.num_channels = stream_info.num_channels;
  header.sample_format = static_cast<uint8_t>(stream_info.sample_format);
  header.sample_rate = stream_info.sample_rate;
  // In audio message, the header contains a timestamp field, while in request
  // message, it instead contains a frames field.
  header.timestamp_or_frames = packet_info.message_type == MessageType::kAudio
                                   ? packet_info.timestamp_us
                                   : stream_info.frames_per_buffer;
  base::WriteBigEndian(  // Deduct the size of |size| itself.
      data, static_cast<uint16_t>(size - sizeof(uint16_t)));
  DCHECK_EQ(sizeof(header), kTotalHeaderBytes);
  memcpy(data + sizeof(uint16_t),
         reinterpret_cast<const char*>(&header) +
             offsetof(struct PacketHeader, message_type),
         kMessageHeaderBytes);
  return data + kTotalHeaderBytes;
}

bool ReadHeader(const char* data, size_t size, PacketInfo* packet_info) {
  DCHECK(packet_info);
  if (size < kMessageHeaderBytes) {
    LOG(ERROR) << "Message doesn't have a complete header.";
    return false;
  }
  PacketHeader header;
  memcpy(reinterpret_cast<char*>(&header) +
             offsetof(struct PacketHeader, message_type),
         data, kMessageHeaderBytes);
  if (!HasPacketHeader(static_cast<MessageType>(header.message_type)) ||
      header.stream_type > static_cast<uint8_t>(StreamType::kLastType) ||
      header.sample_format > static_cast<uint8_t>(SampleFormat::LAST_FORMAT)) {
    LOG(ERROR) << "Invalid message header.";
    return false;
  }
  if (header.num_channels > ::media::limits::kMaxChannels) {
    LOG(ERROR) << "Invalid number of channels: " << header.num_channels;
    return false;
  }
  packet_info->message_type = static_cast<MessageType>(header.message_type);
  packet_info->stream_info.stream_type =
      static_cast<StreamType>(header.stream_type);
  packet_info->stream_info.num_channels = header.num_channels;
  packet_info->stream_info.sample_format =
      static_cast<SampleFormat>(header.sample_format);
  packet_info->stream_info.sample_rate = header.sample_rate;
  if (packet_info->message_type == MessageType::kAck) {
    packet_info->stream_info.frames_per_buffer = header.timestamp_or_frames;
  } else {
    packet_info->timestamp_us = header.timestamp_or_frames;
  }
  return true;
}

scoped_refptr<net::IOBufferWithSize> MakeMessage(const PacketInfo& packet_info,
                                                 const char* data,
                                                 size_t data_size) {
  if (!HasPacketHeader(packet_info.message_type)) {
    LOG(ERROR) << "Only kAck and kAudio message have packet header, use "
                  "MakeMetadataMessage otherwise.";
    return nullptr;
  }
  const size_t total_size = kTotalHeaderBytes + data_size;
  DCHECK_LE(total_size, std::numeric_limits<uint16_t>::max());
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(total_size);
  char* ptr = PopulateHeader(io_buffer->data(), io_buffer->size(), packet_info);
  if (!ptr) {
    return nullptr;
  }
  if (packet_info.message_type == MessageType::kAudio && data_size > 0) {
    DCHECK(data);
    std::copy(data, data + data_size, ptr);
  }
  return io_buffer;
}

scoped_refptr<net::IOBufferWithSize> MakeMetadataMessage(const char* data,
                                                         size_t data_size) {
  if (data == nullptr || data_size == 0) {
    LOG(ERROR) << "Invalid data pointer or size: " << data << ", " << data_size
               << ".";
    return nullptr;
  }

  const uint8_t message_type = static_cast<uint8_t>(MessageType::kMetadata);
  const uint16_t message_size = sizeof(message_type) + data_size;
  DCHECK_LE(message_size, std::numeric_limits<uint16_t>::max());
  auto io_buffer = base::MakeRefCounted<net::IOBufferWithSize>(
      sizeof(message_size) + message_size);

  char* ptr = io_buffer->data();
  base::WriteBigEndian(ptr, message_size);
  ptr += sizeof(message_size);
  memcpy(ptr, &message_type, sizeof(message_type));
  ptr += sizeof(message_type);

  std::copy(data, data + data_size, ptr);
  return io_buffer;
}

bool ReadDataToAudioBus(const StreamInfo& stream_info,
                        const char* data,
                        size_t size,
                        ::media::AudioBus* audio_bus) {
  DCHECK(audio_bus);
  DCHECK_EQ(stream_info.num_channels, audio_bus->channels());
  return ConvertData(stream_info.num_channels, stream_info.sample_format,
                     data + kMessageHeaderBytes, size - kMessageHeaderBytes,
                     audio_bus);
}

size_t DataSizeInBytes(const StreamInfo& stream_info) {
  switch (stream_info.sample_format) {
    case SampleFormat::INTERLEAVED_INT16:
    case SampleFormat::PLANAR_INT16:
      return sizeof(int16_t) * stream_info.num_channels *
             stream_info.frames_per_buffer;
    case SampleFormat::INTERLEAVED_INT32:
    case SampleFormat::PLANAR_INT32:
      return sizeof(int32_t) * stream_info.num_channels *
             stream_info.frames_per_buffer;
    case SampleFormat::INTERLEAVED_FLOAT:
    case SampleFormat::PLANAR_FLOAT:
      return sizeof(float) * stream_info.num_channels *
             stream_info.frames_per_buffer;
  }
}

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast
