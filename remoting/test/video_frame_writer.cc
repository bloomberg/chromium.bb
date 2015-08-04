// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/video_frame_writer.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace {

const base::FilePath::CharType kFrameFileName[] =
    FILE_PATH_LITERAL("frame.png");
const base::FilePath::CharType kRemotingFolder[] =
    FILE_PATH_LITERAL("remoting");
const base::FilePath::CharType kDumpFrameFolder[] =
    FILE_PATH_LITERAL("dumped_images");

// Used to create a unique folder path.
const char kDateAndTimeFormatString[] = "%d-%d-%d_%d-%d-%d";

}  // namespace

namespace remoting {
namespace test {

VideoFrameWriter::VideoFrameWriter()
    : instance_creation_time_(base::Time::Now()),
      frame_name_unique_number_(0) {}

VideoFrameWriter::~VideoFrameWriter() {}

void VideoFrameWriter::WriteFrameToPath(const webrtc::DesktopFrame& frame,
                                        const base::FilePath& image_path) {
  unsigned char* frame_data = reinterpret_cast<unsigned char*>(frame.data());
  std::vector<unsigned char> png_encoded_data;

  if (!gfx::PNGCodec::Encode(
          frame_data, gfx::PNGCodec::FORMAT_BGRA,
          gfx::Size(frame.size().width(), frame.size().height()),
          frame.stride(), true, std::vector<gfx::PNGCodec::Comment>(),
          &png_encoded_data)) {
    LOG(WARNING) << "Failed to encode frame to PNG file";
    return;
  }

  // Dump contents (unsigned chars) to a file as a sequence of chars.
  int write_bytes = base::WriteFile(
      image_path, reinterpret_cast<char*>(&*png_encoded_data.begin()),
      static_cast<int>(png_encoded_data.size()));
  if (write_bytes != static_cast<int>(png_encoded_data.size())) {
    LOG(WARNING) << "Failed to write frame to disk";
  }
}

// Save video frame to path named with the |instance_creation_time|.
void VideoFrameWriter::WriteFrameToDefaultPath(
    const webrtc::DesktopFrame& frame) {
  base::FilePath dump_frame_file_path;
  if (!GetTempDir(&dump_frame_file_path)) {
    LOG(WARNING) << "Failed to retrieve temporary directory path";
    return;
  }

  dump_frame_file_path = dump_frame_file_path.Append(kRemotingFolder);
  dump_frame_file_path = dump_frame_file_path.Append(kDumpFrameFolder);
  if (!CreateDirectoryIfNotExists(dump_frame_file_path)) {
    return;
  }

  // Create a sub-folder using date and time to identify a particular test run.
  dump_frame_file_path = AppendCreationDateAndTime(dump_frame_file_path);
  if (!CreateDirectoryIfNotExists(dump_frame_file_path)) {
    return;
  }

  dump_frame_file_path = dump_frame_file_path.Append(kFrameFileName);

  dump_frame_file_path = dump_frame_file_path.InsertBeforeExtensionASCII(
      base::StringPrintf("(%d)", ++frame_name_unique_number_));

  LOG(INFO) << "Video frame dumped to: " << dump_frame_file_path.value();

  WriteFrameToPath(frame, dump_frame_file_path);
}

base::FilePath VideoFrameWriter::AppendCreationDateAndTime(
    const base::FilePath& file_path) {
  base::Time::Exploded exploded_time;
  instance_creation_time_.LocalExplode(&exploded_time);

  int year = exploded_time.year;
  int month = exploded_time.month;
  int day = exploded_time.day_of_month;
  int hour = exploded_time.hour;
  int minute = exploded_time.minute;
  int second = exploded_time.second;

  return file_path.AppendASCII(base::StringPrintf(
      kDateAndTimeFormatString, year, month, day, hour, minute, second));
}

bool VideoFrameWriter::CreateDirectoryIfNotExists(
    const base::FilePath& file_path) {
  if (!base::DirectoryExists(file_path) && !base::CreateDirectory(file_path)) {
    LOG(WARNING) << "Failed to create directory: " << file_path.value();
    return false;
  }
  return true;
}

}  // namespace test
}  // namespace remoting
