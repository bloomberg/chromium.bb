// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextCodecReplacement_h
#define TextCodecReplacement_h

#include "platform/wtf/text/TextCodec.h"
#include "platform/wtf/text/TextCodecUTF8.h"

namespace WTF {

// The "replacement" encoding exists to prevent attacks that abuse a mismatch
// between encodings supported on the server and the client. The encoder is
// the same as UTF-8; and for a non-empty input the decoder emits U+FFFD and
// terminates. See: https://encoding.spec.whatwg.org/#replacement and
// https://encoding.spec.whatwg.org/#output-encodings
class TextCodecReplacement final : public TextCodecUTF8 {
 public:
  TextCodecReplacement();

  static void RegisterEncodingNames(EncodingNameRegistrar);
  static void RegisterCodecs(TextCodecRegistrar);

 private:
  String Decode(const char*,
                size_t length,
                FlushBehavior,
                bool stop_on_error,
                bool& saw_error) override;

  bool replacement_error_returned_;
};

}  // namespace WTF

#endif  // TextCodecReplacement_h
