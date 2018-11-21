// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/test/fuzzed_data_provider.h"
#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider data_provider(data, size);

  CompactEncDet::TextCorpusType corpus =
      static_cast<CompactEncDet::TextCorpusType>(
          data_provider.ConsumeInt32InRange(0, CompactEncDet::NUM_CORPA));
  Encoding encoding_hint = static_cast<Encoding>(
      data_provider.ConsumeInt32InRange(0, NUM_ENCODINGS));
  Language langauge_hint = static_cast<Language>(
      data_provider.ConsumeInt32InRange(0, NUM_LANGUAGES));
  bool ignore_7bit_mail_encodings = data_provider.ConsumeBool();

  std::vector<char> text = data_provider.ConsumeRemainingBytes<char>();
  int bytes_consumed = 0;
  bool is_reliable = false;
  CompactEncDet::DetectEncoding(
      text.data(), text.size(), nullptr /* url_hint */,
      nullptr /* http_charset_hint */, nullptr /* meta_charset_hint */,
      encoding_hint, langauge_hint, corpus, ignore_7bit_mail_encodings,
      &bytes_consumed, &is_reliable);

  return 0;
}
