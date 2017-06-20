// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextResourceDecoderForFuzzing_h
#define TextResourceDecoderForFuzzing_h

#include "core/html/parser/TextResourceDecoder.h"

#include "platform/testing/FuzzedDataProvider.h"
#include "platform/wtf/text/TextEncoding.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class TextResourceDecoderForFuzzing : public TextResourceDecoder {
 public:
  // Note: Charsets can be long (see the various encodings in wtf/text). For
  // instance: "unicode-1-1-utf-8". To ensure good coverage, set a generous max
  // limit for these sizes (32 bytes should be good).
  TextResourceDecoderForFuzzing(FuzzedDataProvider& fuzzed_data)
      : TextResourceDecoder(static_cast<TextResourceDecoder::ContentType>(
                                fuzzed_data.ConsumeInt32InRange(
                                    TextResourceDecoder::kPlainTextContent,
                                    TextResourceDecoder::kMaxContentType)),
                            WTF::TextEncoding(String::FromUTF8(
                                fuzzed_data.ConsumeBytesInRange(0, 32))),
                            FuzzedOption(fuzzed_data),
                            KURL()) {}

 private:
  static TextResourceDecoder::EncodingDetectionOption FuzzedOption(
      FuzzedDataProvider& fuzzed_data) {
    // Don't use AlwaysUseUTF8ForText which requires knowing the mimeType
    // ahead of time.
    return fuzzed_data.ConsumeBool() ? kUseAllAutoDetection
                                     : kUseContentAndBOMBasedDetection;
  }
};

}  // namespace blink

#endif  // TextResourceDecoderForFuzzing_h
