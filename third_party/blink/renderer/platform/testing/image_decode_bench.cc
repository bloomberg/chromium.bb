// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a minimal wrapping of the Blink image decoders. Used to perform
// a non-threaded, memory-to-memory image decode using micro second accuracy
// clocks to measure image decode time. Basic usage:
//
//   % ninja -C out/Release image_decode_bench &&
//      ./out/Release/image_decode_bench file [iterations [-v,-verbose]]
//
// TODO(noel): Consider adding md5 checksum support to WTF. Use it to compute
// the decoded image frame md5 and output that value.
//
// TODO(noel): Consider integrating this tool in Chrome telemetry for realz,
// using the image corpora used to assess Blink image decode performance. See
// http://crbug.com/398235#c103 and http://crbug.com/258324#c5

#include <fstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/perf/perf_test.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

constexpr size_t kDefaultDecodeTimes = 1;

scoped_refptr<SharedBuffer> ReadFile(const char* name) {
  std::string file_contents;
  const auto file_path = base::FilePath(name);
  if (!base::ReadFileToString(file_path, &file_contents))
    PLOG(FATAL) << "file name: " << name;
  return SharedBuffer::Create(file_contents.data(), file_contents.size());
}

struct ImageMeta {
  char* name;
  int width;
  int height;
  int frames;
};

void PrintErrorAndExit(ImageMeta* image) {
  LOG_ASSERT(false) << "Failed to decode image " << image->name;
}

base::TimeDelta DecodeImageData(SharedBuffer* data, ImageMeta* image) {
  constexpr bool data_complete = true;
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, data_complete, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::Ignore());

  base::ElapsedTimer timer;
  bool all_data_received = true;
  decoder->SetData(data, all_data_received);

  size_t frame_count = decoder->FrameCount();
  for (size_t index = 0; index < frame_count; ++index) {
    if (!decoder->DecodeFrameBufferAtIndex(index))
      PrintErrorAndExit(image);
  }

  const auto elapsed_time = timer.Elapsed();

  if (!frame_count || decoder->Failed())
    PrintErrorAndExit(image);

  image->width = decoder->Size().Width();
  image->height = decoder->Size().Height();
  image->frames = frame_count;
  return elapsed_time;
}

}  // namespace

void ImageDecodeBenchMain(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const char* program = argv[0];

  if (argc < 2) {
    LOG(INFO) << "Usage: " << program << " file [iterations [-v,-verbose]]";
    exit(1);
  }

  // Control bench decode iterations.
  size_t decode_iterations = kDefaultDecodeTimes;
  bool verbose_mode = false;
  if (argc >= 3) {
    char* end = nullptr;
    decode_iterations = strtol(argv[2], &end, 10);
    if (*end != '\0' || !decode_iterations) {
      LOG_ASSERT(false) << "Second argument should be number of iterations. "
                        << "The default is 1. You supplied " << argv[2];
    }

    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    DCHECK(cmd_line);
    verbose_mode = cmd_line->HasSwitch("v") || cmd_line->HasSwitch("verbose");
  }

  std::unique_ptr<Platform> platform = std::make_unique<Platform>();
  Platform::CreateMainThreadAndInitialize(platform.get());

  // Read entire file content into |data| (a contiguous block of memory) then
  // decode it to verify the image and record its ImageMeta data.
  ImageMeta image = {.name = argv[1]};
  scoped_refptr<SharedBuffer> data = ReadFile(argv[1]);
  DecodeImageData(data.get(), &image);

  // Image decode bench for |decode_iterations|.
  base::TimeDelta total_time;
  for (size_t i = 0; i < decode_iterations; ++i)
    total_time += DecodeImageData(data.get(), &image);

  if (verbose_mode) {
    perf_test::PrintResult(
        "decode_time ", argv[1],
        base::StringPrintf("%zu times %d pixels (%dx%d)", decode_iterations,
                           image.width * image.height, image.width,
                           image.height),
        total_time.InMillisecondsF() / decode_iterations, "ms/sample", true);
  } else {
    printf("%f %f\n", total_time.InSecondsF(),
           total_time.InSecondsF() / decode_iterations);
  }
}

}  // namespace blink

int main(int argc, char* argv[]) {
  base::MessageLoop message_loop;
  mojo::core::Init();
  base::CommandLine::Init(argc, argv);
  LOG_ASSERT(logging::InitLogging(logging::LoggingSettings()));

  blink::ImageDecodeBenchMain(argc, argv);
}
