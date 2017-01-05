// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO (scroggo): Move this to
// third_party/WebKit/Source/platform/image-decoders ?

// Compile with:
// gn gen out/Fuzz '--args=use_libfuzzer=true is_asan=true
// is_debug=false is_ubsan_security=true' --check
// ninja -C out/Fuzz blink_png_decoder_fuzzer
//
// Run with:
// ./out/Fuzz/blink_png_decoder_fuzzer
// third_party/WebKit/LayoutTests/images/resources/pngfuzz
//
// Alternatively, it can be run with:
// ./out/Fuzz/blink_png_decoder_fuzzer ~/another_dir_to_store_corpus
// third_party/WebKit/LayoutTests/images/resources/pngfuzz
//
// so the fuzzer will read both directories passed, but all new generated
// testcases will go into ~/another_dir_to_store_corpus
//
// For more details, see
// https://chromium.googlesource.com/chromium/src/+/master/testing/libfuzzer/README.md

#include "platform/image-decoders/png/PNGImageDecoder.cpp"
#include "platform/testing/BlinkFuzzerTestSupport.h"

namespace blink {

std::unique_ptr<ImageDecoder> createDecoder(
    ImageDecoder::AlphaOption alphaOption) {
  return WTF::wrapUnique(new PNGImageDecoder(
      alphaOption, ColorBehavior::transformToTargetForTesting(),
      ImageDecoder::noDecodedImageByteLimit));
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  auto buffer = SharedBuffer::create(data, size);
  // TODO (scroggo): Also test ImageDecoder::AlphaNotPremultiplied?
  auto decoder = createDecoder(ImageDecoder::AlphaPremultiplied);
  const bool allDataReceived = true;
  decoder->setData(buffer.get(), allDataReceived);
  decoder->frameCount();
  if (decoder->failed())
    return 0;
  for (size_t frame = 0; frame < decoder->frameCount(); frame++) {
    decoder->frameBufferAtIndex(frame);
    if (decoder->failed())
      return 0;
  }
  return 0;
}

}  // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return blink::LLVMFuzzerTestOneInput(data, size);
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  blink::InitializeBlinkFuzzTest(argc, argv);
  return 0;
}
