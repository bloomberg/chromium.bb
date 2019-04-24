// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_player/video.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/values.h"

#define VLOGF(level) VLOG(level) << __func__ << "(): "

namespace media {
namespace test {

// Suffix to append to the video file path to get the metadata file path.
constexpr const base::FilePath::CharType* kMetadataSuffix =
    FILE_PATH_LITERAL(".json");

base::FilePath Video::test_data_path_ = base::FilePath();

Video::Video(const base::FilePath& file_path) : file_path_(file_path) {}

Video::~Video() {}

bool Video::Load() {
  // TODO(dstaessens@) Investigate reusing existing infrastructure such as
  //                   DecoderBuffer.
  DCHECK(!file_path_.empty());
  DCHECK(data_.empty());

  // The specified path can be either an absolute path, a path relative to the
  // current directory, or relative to the test data path.
  if (!file_path_.IsAbsolute()) {
    if (!PathExists(file_path_))
      file_path_ = test_data_path_.Append(file_path_);
    file_path_ = base::MakeAbsoluteFilePath(file_path_);
  }
  VLOGF(2) << "File path: " << file_path_;

  int64_t file_size;
  if (!base::GetFileSize(file_path_, &file_size) || (file_size < 0)) {
    VLOGF(1) << "Failed to read file size: " << file_path_;
    return false;
  }

  std::vector<uint8_t> data(file_size);
  if (base::ReadFile(file_path_, reinterpret_cast<char*>(data.data()),
                     base::checked_cast<int>(file_size)) != file_size) {
    VLOGF(1) << "Failed to read file: " << file_path_;
    return false;
  }

  data_ = std::move(data);

  if (!LoadMetadata()) {
    VLOGF(1) << "Failed to load metadata";
    return false;
  }

  return true;
}

bool Video::IsLoaded() const {
  return data_.size() > 0;
}

const base::FilePath& Video::FilePath() const {
  return file_path_;
}

const std::vector<uint8_t>& Video::Data() const {
  return data_;
}

VideoCodec Video::Codec() const {
  return codec_;
}

VideoCodecProfile Video::Profile() const {
  return profile_;
}

uint32_t Video::NumFrames() const {
  return num_frames_;
}

uint32_t Video::NumFragments() const {
  return num_fragments_;
}

gfx::Size Video::Resolution() const {
  return resolution_;
}

const std::vector<std::string>& Video::FrameChecksums() const {
  return frame_checksums_;
}

const std::vector<std::string>& Video::ThumbnailChecksums() const {
  return thumbnail_checksums_;
}

// static
void Video::SetTestDataPath(const base::FilePath& test_data_path) {
  test_data_path_ = test_data_path;
}

bool Video::LoadMetadata() {
  if (IsMetadataLoaded()) {
    VLOGF(1) << "Video metadata is already loaded";
    return false;
  }

  const base::FilePath json_path = file_path_.AddExtension(kMetadataSuffix);
  VLOGF(2) << "File path: " << json_path;

  if (!base::PathExists(json_path)) {
    VLOGF(1) << "Video metadata file not found: " << json_path;
    return false;
  }

  std::string json_data;
  if (!base::ReadFileToString(json_path, &json_data)) {
    VLOGF(1) << "Failed to read video metadata file: " << json_path;
    return false;
  }

  base::JSONReader reader;
  std::unique_ptr<base::Value> metadata(
      reader.ReadToValueDeprecated(json_data));
  if (!metadata) {
    VLOGF(1) << "Failed to parse video metadata: " << json_path << ": "
             << reader.GetErrorMessage();
    return false;
  }

  const base::Value* profile =
      metadata->FindKeyOfType("profile", base::Value::Type::STRING);
  if (!profile) {
    VLOGF(1) << "Key \"profile\" is not found in " << json_path;
    return false;
  }
  profile_ = ConvertStringtoProfile(profile->GetString());
  codec_ = ConvertProfileToCodec(profile_);
  if (profile_ == VIDEO_CODEC_PROFILE_UNKNOWN || codec_ == kUnknownVideoCodec) {
    VLOGF(1) << profile->GetString() << " is not supported";
    return false;
  }

  const base::Value* num_frames =
      metadata->FindKeyOfType("num_frames", base::Value::Type::INTEGER);
  if (!num_frames) {
    VLOGF(1) << "Key \"num_frames\" is not found in " << json_path;
    return false;
  }
  num_frames_ = static_cast<uint32_t>(num_frames->GetInt());

  const base::Value* num_fragments =
      metadata->FindKeyOfType("num_fragments", base::Value::Type::INTEGER);
  if (!num_fragments) {
    VLOGF(1) << "Key \"num_fragments\" is not found in " << json_path;
    return false;
  }
  num_fragments_ = static_cast<uint32_t>(num_fragments->GetInt());

  const base::Value* width =
      metadata->FindKeyOfType("width", base::Value::Type::INTEGER);
  if (!width) {
    VLOGF(1) << "Key \"width\" is not found in " << json_path;
    return false;
  }
  const base::Value* height =
      metadata->FindKeyOfType("height", base::Value::Type::INTEGER);
  if (!height) {
    VLOGF(1) << "Key \"height\" is not found in " << json_path;
    return false;
  }
  resolution_ = gfx::Size(static_cast<uint32_t>(width->GetInt()),
                          static_cast<uint32_t>(height->GetInt()));

  const base::Value* md5_checksums =
      metadata->FindKeyOfType("md5_checksums", base::Value::Type::LIST);
  if (!md5_checksums) {
    VLOGF(1) << "Key \"md5_checksums\" is not found in " << json_path;
    return false;
  }
  for (const base::Value& checksum : md5_checksums->GetList()) {
    frame_checksums_.push_back(checksum.GetString());
  }

  const base::Value* thumbnail_checksums =
      metadata->FindKeyOfType("thumbnail_checksums", base::Value::Type::LIST);
  if (!thumbnail_checksums) {
    VLOGF(1) << "Key \"thumbnail_checksums\" is not found in " << json_path;
    return false;
  }
  for (const base::Value& checksum : thumbnail_checksums->GetList()) {
    const std::string& checksum_str = checksum.GetString();
    if (checksum_str.size() > 0 && checksum_str[0] != '#')
      thumbnail_checksums_.push_back(checksum_str);
  }

  return true;
}

bool Video::IsMetadataLoaded() const {
  return profile_ != VIDEO_CODEC_PROFILE_UNKNOWN || num_frames_ != 0;
}

// static
VideoCodecProfile Video::ConvertStringtoProfile(const std::string& profile) {
  if (profile == "H264PROFILE_MAIN") {
    return H264PROFILE_MAIN;
  } else if (profile == "VP8PROFILE_ANY") {
    return VP8PROFILE_ANY;
  } else if (profile == "VP9PROFILE_PROFILE0") {
    return VP9PROFILE_PROFILE0;
  } else if (profile == "VP9PROFILE_PROFILE2") {
    return VP9PROFILE_PROFILE2;
  } else {
    VLOG(2) << profile << " is not supported";
    return VIDEO_CODEC_PROFILE_UNKNOWN;
  }
}

// static
VideoCodec Video::ConvertProfileToCodec(VideoCodecProfile profile) {
  if (profile >= H264PROFILE_MIN && profile <= H264PROFILE_MAX) {
    return kCodecH264;
  } else if (profile >= VP8PROFILE_MIN && profile <= VP8PROFILE_MAX) {
    return kCodecVP8;
  } else if (profile >= VP9PROFILE_MIN && profile <= VP9PROFILE_MAX) {
    return kCodecVP9;
  } else {
    VLOG(2) << GetProfileName(profile) << " is not supported";
    return kUnknownVideoCodec;
  }
}

}  // namespace test
}  // namespace media
