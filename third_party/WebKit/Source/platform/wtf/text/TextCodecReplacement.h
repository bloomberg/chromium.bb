// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextCodecReplacement_h
#define TextCodecReplacement_h

#include "platform/wtf/text/TextCodec.h"
#include "platform/wtf/text/TextCodecUTF8.h"

namespace WTF {

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
