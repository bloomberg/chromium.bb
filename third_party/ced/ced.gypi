# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'ced_sources': [
      # find src -maxdepth 3 ! -type d | egrep '\.(h|cc)$' | grep -v test.cc | \
      # LC_COLLATE=c sort | sed "s/^\(.*\)$/      '\1',/"
      'src/compact_enc_det/compact_enc_det.cc',
      'src/compact_enc_det/compact_enc_det.h',
      'src/compact_enc_det/compact_enc_det_generated_tables2.h',
      'src/compact_enc_det/compact_enc_det_generated_tables.h',
      'src/compact_enc_det/compact_enc_det_hint_code.cc',
      'src/compact_enc_det/compact_enc_det_hint_code.h',
      'src/util/basictypes.h',
      'src/util/case_insensitive_hash.h',
      'src/util/commandlineflags.h',
      'src/util/encodings/encodings.cc',
      'src/util/encodings/encodings.h',
      'src/util/encodings/encodings.pb.h',
      'src/util/languages/languages.cc',
      'src/util/languages/languages.h',
      'src/util/languages/languages.pb.h',
      'src/util/logging.h',
      'src/util/port.h',
      'src/util/string_util.h',
      'src/util/varsetter.h',
    ],
  }
}
