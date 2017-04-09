// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptPromiseProperties_h
#define ScriptPromiseProperties_h

// See ScriptPromiseProperty.h
#define SCRIPT_PROMISE_PROPERTIES(P, ...)    \
  P(ScriptPromise, kReady##__VA_ARGS__)      \
  P(ScriptPromise, kClosed##__VA_ARGS__)     \
  P(ScriptPromise, kFinished##__VA_ARGS__)   \
  P(ScriptPromise, kLoaded##__VA_ARGS__)     \
  P(ScriptPromise, kReleased##__VA_ARGS__)   \
  P(ScriptPromise, kUserChoice##__VA_ARGS__) \
  P(ScriptPromise, kPreloadResponse##__VA_ARGS__)

#endif  // ScriptPromiseProperties_h
