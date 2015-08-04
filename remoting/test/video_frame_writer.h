// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_VIDEO_FRAME_WRITER_H_
#define REMOTING_TEST_VIDEO_FRAME_WRITER_H_

#include "base/time/time.h"

namespace base {
class FilePath;
}

namespace webrtc {
class DesktopFrame;
}

namespace remoting {
namespace test {

// A helper class to dump video frames to disk.
class VideoFrameWriter {
 public:
  VideoFrameWriter();
  ~VideoFrameWriter();

  // Save video frame to a local path.
  void WriteFrameToPath(const webrtc::DesktopFrame& frame,
                        const base::FilePath& image_path);

  // Save video frame to path named with the |instance_creation_time|.
  void WriteFrameToDefaultPath(const webrtc::DesktopFrame& frame);

 private:
  // Returns a FilePath by appending the creation time of this object.
  base::FilePath AppendCreationDateAndTime(const base::FilePath& file_path);

  // Returns true if directory already exists or it was created successfully.
  bool CreateDirectoryIfNotExists(const base::FilePath& file_path);

  // Used to create a unique folder to dump video frames.
  const base::Time instance_creation_time_;

  // Used to append before file extension to create unique file name.
  int frame_name_unique_number_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameWriter);
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_VIDEO_FRAME_WRITER_H_
