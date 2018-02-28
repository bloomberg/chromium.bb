// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletProcessorErrorState_h
#define AudioWorkletProcessorErrorState_h

namespace blink {

// A list of state regarding the error in AudioWorkletProcessor object.
enum class AudioWorkletProcessorErrorState : unsigned {
  // The constructor or the process method in the processor has not thrown any
  // exception.
  kNoError = 0,

  // An exception thrown from the construction failure.
  kConstructionError = 1,

  // An exception thrown from the process method.
  kProcessError = 2,
};

}  // namespace blink

#endif
