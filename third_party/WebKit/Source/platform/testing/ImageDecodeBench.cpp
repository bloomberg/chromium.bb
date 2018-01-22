// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides a minimal wrapping of the Blink image decoders. Used to perform
// a non-threaded, memory-to-memory image decode using micro second accuracy
// clocks to measure image decode time. Optionally applies color correction
// during image decoding on supported platforms (default off). Usage:
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
#include "ui/gfx/test/icc_profiles.h"

namespace blink {

namespace {

scoped_refptr<SharedBuffer> ReadFile(const char* name) {
  std::ifstream file(name, std::ios::in | std::ios::binary);
  if (!file) {
    fprintf(stderr, "Can't open file %s\n", name);
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

bool DecodeImageData(SharedBuffer* data,
                     bool color_correction,
                     size_t packet_size) {
  std::unique_ptr<ImageDecoder> decoder =
      ImageDecoder::Create(data, true, ImageDecoder::kAlphaPremultiplied,
                           color_correction ? ColorBehavior::TransformToSRGB()
                                            : ColorBehavior::Ignore());
  if (!packet_size) {
    bool all_data_received = true;
    decoder->SetData(data, all_data_received);

    int frame_count = decoder->FrameCount();
    for (int i = 0; i < frame_count; ++i) {
      if (!decoder->DecodeFrameBufferAtIndex(i))
        return false;
    }

    return !decoder->Failed();
  }

  scoped_refptr<SharedBuffer> packet_data = SharedBuffer::Create();
  size_t position = 0;
  size_t next_frame_to_decode = 0;
  while (true) {
    const char* packet;
    size_t length = data->GetSomeData(packet, position);

    length = std::min(length, packet_size);
    packet_data->Append(packet, length);
    position += length;

    bool all_data_received = position == data->size();
    decoder->SetData(packet_data.get(), all_data_received);

    size_t frame_count = decoder->FrameCount();
    for (; next_frame_to_decode < frame_count; ++next_frame_to_decode) {
      ImageFrame* frame =
          decoder->DecodeFrameBufferAtIndex(next_frame_to_decode);
      if (frame->GetStatus() != ImageFrame::kFrameComplete)
        break;
    }

    if (all_data_received || decoder->Failed())
      break;
  }

  return !decoder->Failed();
}

}  // namespace

int Main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);

  // If the platform supports color correction, allow it to be controlled.

  bool apply_color_correction = false;

  if (argc >= 2 && strcmp(argv[1], "--color-correct") == 0) {
    apply_color_correction = (--argc, ++argv, true);
  }

  if (argc < 2) {
    fprintf(stderr,
            "Usage: %s [--color-correct] file [iterations] [packetSize]\n",
            argv[0]);
    exit(1);
  }

  // Control decode bench iterations and packet size.

  size_t iterations = 1;
  if (argc >= 3) {
    char* end = nullptr;
    iterations = strtol(argv[2], &end, 10);
    if (*end != '\0' || !iterations) {
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

  // Create a web platform.  blink::Platform can't be used directly because its
  // constructor is protected.

  class WebPlatform : public blink::Platform {};

  Platform::Initialize(new WebPlatform());

  // Read entire file content to data, and consolidate the SharedBuffer data
  // segments into one, contiguous block of memory.

  scoped_refptr<SharedBuffer> data = ReadFile(argv[1]);
  if (!data.get() || !data->size()) {
    fprintf(stderr, "Error reading image data from [%s]\n", argv[1]);
    exit(2);
  }

  data->Data();

  // Warm-up: throw out the first iteration for more consistent results.

  if (!DecodeImageData(data.get(), apply_color_correction, packet_size)) {
    fprintf(stderr, "Image decode failed [%s]\n", argv[1]);
    exit(3);
  }

  // Image decode bench for iterations.

  double total_time = 0.0;

  for (size_t i = 0; i < iterations; ++i) {
    auto start_time = CurrentTimeTicks();
    bool decoded =
        DecodeImageData(data.get(), apply_color_correction, packet_size);
    double elapsed_time = (CurrentTimeTicks() - start_time).InSecondsF();
    total_time += elapsed_time;
    if (!decoded) {
      fprintf(stderr, "Image decode failed [%s]\n", argv[1]);
      exit(3);
    }
  }

  // Results to stdout.

  double average_time = total_time / static_cast<double>(iterations);
  printf("%f %f\n", total_time, average_time);
  return 0;
}

}  // namespace blink

int main(int argc, char* argv[]) {
  base::MessageLoop message_loop;
  mojo::edk::Init();
  return blink::Main(argc, argv);
}
