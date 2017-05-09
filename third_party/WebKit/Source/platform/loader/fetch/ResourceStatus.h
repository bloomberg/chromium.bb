// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResourceStatus_h
#define ResourceStatus_h

namespace blink {

enum class ResourceStatus : uint8_t {
  kNotStarted,
  kPending,  // load in progress
  kCached,   // load completed successfully
  kLoadError,
  kDecodeError
};

}  // namespace blink

#endif
