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
  TextResourceDecoderForFuzzing(FuzzedDataProvider& fuzzed_data)
      : TextResourceDecoder(FuzzedOption(fuzzed_data)) {}

 private:
  static TextResourceDecoderOptions FuzzedOption(
      FuzzedDataProvider& fuzzed_data) {
    switch (static_cast<TextResourceDecoderOptions::EncodingDetectionOption>(
        fuzzed_data.ConsumeInt32InRange(
            TextResourceDecoderOptions::kUseAllAutoDetection,
            TextResourceDecoderOptions::kAlwaysUseUTF8ForText))) {
      case TextResourceDecoderOptions::kUseAllAutoDetection:
        return TextResourceDecoderOptions::CreateWithAutoDetection(
            FuzzedContentType(fuzzed_data), FuzzedEncoding(fuzzed_data),
            WTF::TextEncoding(), NullURL());

      case TextResourceDecoderOptions::kUseContentAndBOMBasedDetection:
        return TextResourceDecoderOptions(FuzzedContentType(fuzzed_data),
                                          FuzzedEncoding(fuzzed_data));

      case TextResourceDecoderOptions::kAlwaysUseUTF8ForText:
        return TextResourceDecoderOptions::CreateAlwaysUseUTF8ForText();
    }
  }

  static TextResourceDecoderOptions::ContentType FuzzedContentType(
      FuzzedDataProvider& fuzzed_data) {
    return static_cast<TextResourceDecoderOptions::ContentType>(
        fuzzed_data.ConsumeInt32InRange(
            TextResourceDecoderOptions::kPlainTextContent,
            TextResourceDecoderOptions::kMaxContentType));
  }

  static WTF::TextEncoding FuzzedEncoding(FuzzedDataProvider& fuzzed_data) {
    // Note: Charsets can be long (see the various encodings in
    // wtf/text). For instance: "unicode-1-1-utf-8". To ensure good coverage,
    // set a generous max limit for these sizes (32 bytes should be good).
    return WTF::TextEncoding(
        String::FromUTF8(fuzzed_data.ConsumeBytesInRange(0, 32)));
  }
};

}  // namespace blink

#endif  // TextResourceDecoderForFuzzing_h
