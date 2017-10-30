// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CharacterRange_h
#define CharacterRange_h

namespace blink {

struct CharacterRange {
  CharacterRange(float from, float to, float ascent, float descent)
      : start(from), end(to), ascent(ascent), descent(descent) {
    DCHECK_LE(start, end);
  }

  float Width() const { return end - start; }
  float Height() const { return ascent + descent; }

  float start;
  float end;

  float ascent;
  float descent;
};

}  // namespace blink

#endif  // CharacterRange_h
