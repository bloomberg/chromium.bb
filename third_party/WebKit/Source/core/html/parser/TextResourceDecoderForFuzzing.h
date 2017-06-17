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
  // Note: mimeTypes can be quite long and still valid for XML. See the
  // comment in DOMImplementation.cpp which says:
  // Per RFCs 3023 and 2045, an XML MIME type is of the form:
  // ^[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+/[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]+\+xml$
  //
  // Similarly, charsets can be long too (see the various encodings in
  // wtf/text). For instance: "unicode-1-1-utf-8". To ensure good coverage,
  // set a generous max limit for these sizes (32 bytes should be good).
  TextResourceDecoderForFuzzing(FuzzedDataProvider& fuzzed_data)
      : TextResourceDecoder(
            String::FromUTF8(fuzzed_data.ConsumeBytesInRange(0, 32)),
            WTF::TextEncoding(
                String::FromUTF8(fuzzed_data.ConsumeBytesInRange(0, 32))),
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
