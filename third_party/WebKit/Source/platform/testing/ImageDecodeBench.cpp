// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a minimal wrapping of the Blink image decoders. Used to perform
// a non-threaded, memory-to-memory image decode using micro second accuracy
// clocks to measure image decode time. Basic usage:
//
//   % ninja -C out/Release image_decode_bench &&
//      ./out/Release/image_decode_bench file [iterations]
//
// TODO(noel): Consider adding md5 checksum support to WTF. Use it to compute
// the decoded image frame md5 and output that value.
//
// TODO(noel): Consider integrating this tool in Chrome telemetry for realz,
// using the image corpora used to assess Blink image decode performance. See
// http://crbug.com/398235#c103 and http://crbug.com/258324#c5

#include <fstream>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder.h"
#include "platform/SharedBuffer.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"

namespace blink {

namespace {

scoped_refptr<SharedBuffer> ReadFile(const char* name) {
  std::ifstream file(name, std::ios::in | std::ios::binary);
  if (!file) {
    fprintf(stderr, "Cannot open file %s\n", name);
    exit(2);
  }

  file.seekg(0, std::ios::end);
  size_t file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  if (!file || !file_size)
    return SharedBuffer::Create();

  Vector<char> buffer(file_size);
  if (!file.read(buffer.data(), file_size)) {
    fprintf(stderr, "Error reading file %s\n", name);
    exit(2);
  }

  return SharedBuffer::AdoptVector(buffer);
}

struct ImageMeta {
  char* name;
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

void DecodeImageData(SharedBuffer* data, size_t packet_size, ImageMeta* image) {
  std::unique_ptr<ImageDecoder> decoder = ImageDecoder::Create(
      data, true, ImageDecoder::kAlphaPremultiplied, ColorBehavior::Ignore());

  if (!packet_size) {
    auto start = CurrentTimeTicks();

    bool all_data_received = true;
    decoder->SetData(data, all_data_received);

    size_t frame_count = decoder->FrameCount();
    for (size_t index = 0; index < frame_count; ++index) {
      if (!decoder->DecodeFrameBufferAtIndex(index))
        DecodeFailure(image);
    }

    image->time += (CurrentTimeTicks() - start).InSecondsF();
    image->width = decoder->Size().Width();
    image->height = decoder->Size().Height();
    image->frames = frame_count;

    if (!frame_count || decoder->Failed())
      DecodeFailure(image);
    return;
  }

  scoped_refptr<SharedBuffer> packet_data = SharedBuffer::Create();
  size_t frame_count = 0;
  size_t position = 0;
  size_t index = 0;

  while (true) {
    const char* packet;
    size_t length = data->GetSomeData(packet, position);

    length = std::min(length, packet_size);
    packet_data->Append(packet, length);
    position += length;

    auto start = CurrentTimeTicks();

    bool all_data_received = (position >= data->size());
    decoder->SetData(packet_data.get(), all_data_received);

    frame_count = decoder->FrameCount();
    for (; index < frame_count; ++index) {
      ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(index);
      if (frame->GetStatus() != ImageFrame::kFrameComplete)
        DecodeFailure(image);
    }

    image->time += (CurrentTimeTicks() - start).InSecondsF();

    if (all_data_received || decoder->Failed())
      break;
  }

  image->width = decoder->Size().Width();
  image->height = decoder->Size().Height();
  image->frames = frame_count;

  if (!frame_count || decoder->Failed())
    DecodeFailure(image);
}

}  // namespace

int ImageDecodeBenchMain(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const char* program = argv[0];

  if (argc < 2) {
    fprintf(stderr, "Usage: %s file [iterations] [packetSize]\n", program);
    exit(1);
  }

  // Control bench decode iterations and packet size.

  size_t decode_iterations = 1;
  if (argc >= 3) {
    char* end = nullptr;
    decode_iterations = strtol(argv[2], &end, 10);
    if (*end != '\0' || !decode_iterations) {
      fprintf(stderr,
              "Second argument should be number of iterations. "
              "The default is 1. You supplied %s\n",
              argv[2]);
      exit(1);
    }
  }

  size_t packet_size = 0;
  if (argc >= 4) {
    char* end = nullptr;
    packet_size = strtol(argv[3], &end, 10);
    if (*end != '\0') {
      fprintf(stderr,
              "Third argument should be packet size. Default is "
              "0, meaning to decode the entire image in one packet. You "
              "supplied %s\n",
              argv[3]);
      exit(1);
    }
  }

  // Create a web platform. blink::Platform can't be used directly because it
  // has a protected constructor.

  class WebPlatform : public Platform {};
  Platform::Initialize(new WebPlatform());

  // Read entire file content to data, and consolidate the SharedBuffer data
  // segments into one, contiguous block of memory.

  scoped_refptr<SharedBuffer> data = ReadFile(argv[1]);
  if (!data.get() || !data->size()) {
    fprintf(stderr, "Error reading image %s\n", argv[1]);
    exit(2);
  }

  data->Data();

  // Warm-up: throw out the first iteration for more consistent results.

  ImageMeta image;
  image.name = argv[1];
  DecodeImageData(data.get(), packet_size, &image);

  // Image decode bench for decode_iterations.

  double total_time = 0.0;
  for (size_t i = 0; i < decode_iterations; ++i) {
    image.time = 0.0;
    DecodeImageData(data.get(), packet_size, &image);
    total_time += image.time;
  }

  // Results to stdout.

  double average_time = total_time / decode_iterations;
  printf("%f %f\n", total_time, average_time);
  return 0;
}

}  // namespace blink

int main(int argc, char* argv[]) {
  base::MessageLoop message_loop;
  mojo::edk::Init();
  return blink::ImageDecodeBenchMain(argc, argv);
}
