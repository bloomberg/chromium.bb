// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CrossOriginAttributeValue_h
#define CrossOriginAttributeValue_h

namespace blink {

enum CrossOriginAttributeValue {
  kCrossOriginAttributeNotSet,
  kCrossOriginAttributeAnonymous,
  kCrossOriginAttributeUseCredentials,
};

}  // namespace blink

#endif
