// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceptClientHints_h
#define AcceptClientHints_h

#include "wtf/text/WTFString.h"

namespace blink {

class LocalFrame;

void handleAcceptClientHintsHeader(const String& headerValue, LocalFrame*);

} // namespace blink
#endif

