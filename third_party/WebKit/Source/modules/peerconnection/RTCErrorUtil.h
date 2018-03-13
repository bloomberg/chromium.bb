// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RTCErrorUtil_h
#define RTCErrorUtil_h

#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebRTCError.h"

namespace blink {

class DOMException;

DOMException* CreateDOMExceptionFromWebRTCError(const WebRTCError&);

}  // namespace blink

#endif  // RTCErrorUtil_h
