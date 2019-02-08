// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a minimal wrapping of the Blink image decoders. Used to perform
// a non-threaded, memory-to-memory image decode using micro second accuracy
// clocks to measure image decode time.
//
// TODO(noel): Consider integrating this tool in Chrome telemetry for realz,
// using the image corpora used to assess Blink image decode performance. See
// http://crbug.com/398235#c103 and http://crbug.com/258324#c5

#include <chrono>
#include <fstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "mojo/core/embedder/embedder.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

namespace blink {

namespace {

scoped_refptr<SharedBuffer> ReadFile(const char* name) {
  std::string file;
  if (base::ReadFileToString(base::FilePath::FromUTF8Unsafe(name), &file))
    return SharedBuffer::Create(file.data(), file.size());
  perror(name);
  exit(2);
  return SharedBuffer::Create();
}

struct ImageMeta {
  const char* name;
  int width;
  int height;
  int frames;
  // Cumulative time in seconds to decode all frames.
  double time;
};

void DecodeFailure(ImageMeta* image) {
  fprintf(stderr, "Failed to decode image %s\n", image->name);
  exit(3);
}

void DecodeImageData(SharedBuffer* data, ImageMeta* image) {
  const bool all_data_received = true;

  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, all_data_received, ImageDecoder::kAlphaPremultiplied,
      ImageDecoder::kDefaultBitDepth, ColorBehavior::Ignore());

  auto start = std::chrono::steady_clock::now();

  decoder->SetData(data, all_data_received);
  size_t frame_count = decoder->FrameCount();
  for (size_t index = 0; index < frame_count; ++index) {
    if (!decoder->DecodeFrameBufferAtIndex(index))
      DecodeFailure(image);
  }

  auto end = std::chrono::steady_clock::now();

  if (!frame_count || decoder->Failed())
    DecodeFailure(image);

  image->time += std::chrono::duration<double>(end - start).count();
  image->width = decoder->Size().Width();
  image->height = decoder->Size().Height();
  image->frames = frame_count;
}

const char* program = nullptr;

base::CommandLine* CommandLineInitialize(int argc, char* argv[]) {
  program = argv[0];
  CHECK(base::CommandLine::Init(argc, argv));
  return base::CommandLine::ForCurrentProcess();
}

void ShowUsageAndExit() {
  fprintf(stderr, "Usage: %s [-i=iterations] file [file...]\n", program);
  exit(1);
}

bool CommandLineHelp(const base::CommandLine* command_line) {
  return command_line->HasSwitch("help") || command_line->HasSwitch("h");
}

unsigned CommandLineIterations(const base::CommandLine* command_line) {
  if (!command_line->HasSwitch("i"))
    return 1;
  int iterations = atoi(command_line->GetSwitchValueASCII("i").c_str());
  if (iterations >= 1)
    return unsigned(iterations);
  ShowUsageAndExit();
  return 0;
}

}  // namespace

int ImageDecodeBenchMain(int argc, char* argv[]) {
  base::CommandLine* command_line = CommandLineInitialize(argc, argv);

  if (CommandLineHelp(command_line) || !command_line->GetArgs().size())
    ShowUsageAndExit();

  const unsigned iterations = CommandLineIterations(command_line);

  // Setup Blink platform.

  std::unique_ptr<Platform> platform = std::make_unique<Platform>();
  Platform::CreateMainThreadAndInitialize(platform.get());

  // Bench each image file.

  for (const auto& file : command_line->GetArgs()) {
    const char* name = file.c_str();

    // Read entire file content into |data| (a contiguous block of memory) then
    // decode it to verify the image and record its ImageMeta data.

    ImageMeta image = {name, 0, 0, 0, 0};
    scoped_refptr<SharedBuffer> data = ReadFile(name);
    DecodeImageData(data.get(), &image);

    // Image decode bench for iterations.

    double total_time = 0.0;
    for (unsigned i = 0; i < iterations; ++i) {
      image.time = 0.0;
      DecodeImageData(data.get(), &image);
      total_time += image.time;
    }

    // Results to stdout.

    double average_time = total_time / iterations;
    printf("%f %f %s\n", total_time, average_time, name);
  }

  return 0;
}

}  // namespace blink

int main(int argc, char* argv[]) {
  base::MessageLoop message_loop;
  mojo::core::Init();
  return blink::ImageDecodeBenchMain(argc, argv);
}
