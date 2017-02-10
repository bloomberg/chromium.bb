// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletProxyClient_h
#define AnimationWorkletProxyClient_h

#include "core/CoreExport.h"
#include "core/dom/CompositorProxyClient.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CORE_EXPORT AnimationWorkletProxyClient : public CompositorProxyClient {
  WTF_MAKE_NONCOPYABLE(AnimationWorkletProxyClient);

 public:
  AnimationWorkletProxyClient() {}
};

}  // namespace blink

#endif  // AnimationWorkletProxyClient_h
