// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTILS_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTILS_H_

#include <cstdint>

#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "media/base/audio_bus.h"
#include "net/base/io_buffer.h"

namespace chromecast {
namespace media {
namespace capture_service {

// Read message header to |packet_info|, and return whether success.
// The header of the message consists of <uint8_t message_type>
// <uint8_t stream_type> <uint8_t channels><uint8_t sample_format>
// <uint16_t sample_rate><uint64_t timestamp_us|frames_per_buffer>.
// If |message_type| is kAudio, it is an audio data message that has
// |timestamp_us|, otherwise if |message_type| is kAck, it's a request
// message that has |frames_per_buffer|.
// Note it cannot be used to read kMetadata messages, which don't have header
// besides |message_type| bits.
// Note |packet_info| will be untouched if fails to read header.
// Note unsigned |timestamp_us| will be converted to signed |timestamp| if
// valid.
// Note |data| here has been parsed firstly by SmallMessageSocket, and
// thus doesn't have <uint16_t size> bits.
bool ReadHeader(const char* data, size_t size, PacketInfo* packet_info);

// Make a IO buffer for stream message. It will populate the header with
// |packet_info|, and copy |data| into the message if packet has audio and
// |data| is not null. The returned buffer will have a length of |data_size| +
// header size. Return nullptr if fails. Caller must guarantee the memory of
// |data| has at least |data_size| when has audio.
// Note buffer will be sent with SmallMessageSocket, and thus contains a uint16
// size field in the very first.
scoped_refptr<net::IOBufferWithSize> MakeMessage(const PacketInfo& packet_info,
                                                 const char* data,
                                                 size_t data_size);

// Make a IO buffer for metadata message. It will populate message size and type
// fields, and copy |data| into the message. The returned buffer will have a
// length of |data_size| + sizeof(uint8_t message_type) + sizeof(uint16_t size).
// Note metadata cannot be empty, and the method will fail and return null if
// |data| is null or |data_size| is zero.
scoped_refptr<net::IOBufferWithSize> MakeMetadataMessage(const char* data,
                                                         size_t data_size);

// Read the audio data in the message and copy to |audio_bus| based on
// |stream_info|. Return false if fails.
bool ReadDataToAudioBus(const StreamInfo& stream_info,
                        const char* data,
                        size_t size,
                        ::media::AudioBus* audio_bus);

// Populate header of the message, including the SmallMessageSocket size bits.
// Note this is used by unittest, user should use MakeMessage directly.
char* PopulateHeader(char* data, size_t size, const PacketInfo& stream_info);

// Return the expected size of the data of a stream message with |stream_info|.
size_t DataSizeInBytes(const StreamInfo& stream_info);

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_MESSAGE_PARSING_UTILS_H_
