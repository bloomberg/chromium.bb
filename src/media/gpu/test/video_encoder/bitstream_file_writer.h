// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_ENCODER_BITSTREAM_FILE_WRITER_H_
#define MEDIA_GPU_TEST_VIDEO_ENCODER_BITSTREAM_FILE_WRITER_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "media/gpu/test/bitstream_helpers.h"

namespace media {
namespace test {
// This class writes a given bitstream to |output_filepath|. The output file
// format is H264 AnnexB format if the bitstream is H264 and IVF if VP8 and VP9.
class BitstreamFileWriter : public BitstreamProcessor {
 public:
  static std::unique_ptr<BitstreamFileWriter> Create(
      const base::FilePath& output_filepath,
      VideoCodec codec,
      const gfx::Size& resolution,
      uint32_t frame_rate,
      uint32_t num_frames,
      base::Optional<size_t> num_vp9_temporal_layers_to_write = base::nullopt);
  BitstreamFileWriter(const BitstreamFileWriter&) = delete;
  BitstreamFileWriter operator=(const BitstreamFileWriter&) = delete;
  ~BitstreamFileWriter() override;

  void ProcessBitstream(scoped_refptr<BitstreamRef> bitstream,
                        size_t frame_index) override;
  bool WaitUntilDone() override;

 private:
  class FrameFileWriter;
  BitstreamFileWriter(std::unique_ptr<FrameFileWriter> frame_file_writer,
                      base::Optional<size_t> num_vp9_temporal_layers_to_write_);
  void WriteBitstreamTask(scoped_refptr<BitstreamRef> bitstream,
                          size_t frame_index);

  const std::unique_ptr<FrameFileWriter> frame_file_writer_;
  const base::Optional<size_t> num_vp9_temporal_layers_to_write_;

  // The number of buffers currently queued for writing.
  size_t num_buffers_writing_ GUARDED_BY(writer_lock_);
  // The number of encountered errors.
  size_t num_errors_ GUARDED_BY(writer_lock_);

  // Thread on which bitstream buffer writing is done.
  base::Thread writer_thread_;
  mutable base::ConditionVariable writer_cv_;
  mutable base::Lock writer_lock_;

  SEQUENCE_CHECKER(writer_thread_sequence_checker_);
};
}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_ENCODER_BITSTREAM_FILE_WRITER_H_
