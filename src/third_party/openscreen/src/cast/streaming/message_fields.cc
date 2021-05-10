// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/message_fields.h"

#include <array>
#include <utility>

#include "util/enum_name_table.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr EnumNameTable<AudioCodec, 2> kAudioCodecNames{
    {{"aac", AudioCodec::kAac}, {"opus", AudioCodec::kOpus}}};

constexpr EnumNameTable<VideoCodec, 4> kVideoCodecNames{
    {{"h264", VideoCodec::kH264},
     {"vp8", VideoCodec::kVp8},
     {"hevc", VideoCodec::kHevc},
     {"vp9", VideoCodec::kVp9}}};

}  // namespace

const char* CodecToString(AudioCodec codec) {
  return GetEnumName(kAudioCodecNames, codec).value();
}

ErrorOr<AudioCodec> StringToAudioCodec(absl::string_view name) {
  return GetEnum(kAudioCodecNames, name);
}

const char* CodecToString(VideoCodec codec) {
  return GetEnumName(kVideoCodecNames, codec).value();
}

ErrorOr<VideoCodec> StringToVideoCodec(absl::string_view name) {
  return GetEnum(kVideoCodecNames, name);
}

}  // namespace cast
}  // namespace openscreen
