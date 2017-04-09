// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DOMHighResTimeStamp_h
#define DOMHighResTimeStamp_h

namespace blink {

typedef double DOMHighResTimeStamp;

inline DOMHighResTimeStamp ConvertSecondsToDOMHighResTimeStamp(double seconds) {
  return static_cast<DOMHighResTimeStamp>(seconds * 1000.0);
}

inline double ConvertDOMHighResTimeStampToSeconds(
    DOMHighResTimeStamp milliseconds) {
  return milliseconds / 1000.0;
}

}  // namespace blink

#endif  // DOMHighResTimeStamp_h
