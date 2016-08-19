// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/TextResourceDecoder.h"

#include "platform/testing/FuzzedDataProvider.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "wtf/text/WTFString.h"
#include <algorithm>

namespace blink {

class TextResourceDecoderForFuzzing : public TextResourceDecoder {
public:
    // Note: mimeTypes can be quite long and still valid for XML. See the
    // comment in DOMImplementation.cpp which says:
    // Per RFCs 3023 and 2045, an XML MIME type is of the form:
    // ^[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+/[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+\+xml$
    //
    // Similarly, charsets can be long too (see the various encodings in
    // wtf/text). For instance: "unicode-1-1-utf-8". To ensure good coverage,
    // set a generous max limit for these sizes (32 bytes should be good).
    TextResourceDecoderForFuzzing(FuzzedDataProvider& fuzzedData)
        : TextResourceDecoder(String::fromUTF8(fuzzedData.ConsumeBytesInRange(0, 32)), String::fromUTF8(fuzzedData.ConsumeBytesInRange(0, 32)), FuzzedOption(fuzzedData))
    {
    }

private:
    static TextResourceDecoder::EncodingDetectionOption FuzzedOption(FuzzedDataProvider& fuzzedData)
    {
        // Don't use AlwaysUseUTF8ForText which requires knowing the mimeType
        // ahead of time.
        return fuzzedData.ConsumeBool() ? UseAllAutoDetection : UseContentAndBOMBasedDetection;
    }
};

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
