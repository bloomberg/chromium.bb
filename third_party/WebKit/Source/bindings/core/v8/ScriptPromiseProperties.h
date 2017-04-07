// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptPromiseProperties_h
#define ScriptPromiseProperties_h

// See ScriptPromiseProperty.h
#define SCRIPT_PROMISE_PROPERTIES(P, ...)   \
  P(ScriptPromise, Ready##__VA_ARGS__)      \
  P(ScriptPromise, Closed##__VA_ARGS__)     \
  P(ScriptPromise, Finished##__VA_ARGS__)   \
  P(ScriptPromise, Loaded##__VA_ARGS__)     \
  P(ScriptPromise, Released##__VA_ARGS__)   \
  P(ScriptPromise, UserChoice##__VA_ARGS__) \
  P(ScriptPromise, PreloadResponse##__VA_ARGS__)

#endif  // ScriptPromiseProperties_h
