// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/TextResourceDecoderForFuzzing.h"

#include "platform/testing/FuzzedDataProvider.h"
#include "platform/testing/TestingPlatformSupport.h"
#include <algorithm>

namespace blink {

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    FuzzedDataProvider fuzzedData(data, size);
    TextResourceDecoderForFuzzing decoder(fuzzedData);
    CString bytes = fuzzedData.ConsumeRemainingBytes();
    decoder.decode(bytes.data(), bytes.length());
    decoder.flush();
    return 0;
}

} // namespace blink

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    return blink::LLVMFuzzerTestOneInput(data, size);
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    // Intentional leak - no need to do cleanup as explained in
    // "Initialization/Cleanup" section of testing/libfuzzer/efficient_fuzzer.md
    DEFINE_STATIC_LOCAL(blink::ScopedUnittestsEnvironmentSetup, testSetup, (*argc, *argv));
    ALLOW_UNUSED_LOCAL(testSetup);

    return 0;
}
